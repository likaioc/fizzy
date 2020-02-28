#include "execute.hpp"
#include "parser.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>
#include <unordered_map>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
constexpr auto JsonExtension = ".json";
constexpr auto UnnamedModule = "_unnamed";

template <typename T>
uint64_t json_to_value(const json& v)
{
    return static_cast<std::make_unsigned_t<T>>(std::stoull(v.get<std::string>()));
}

fizzy::bytes load_wasm_file(const fs::path& json_file_path, std::string_view filename)
{
    std::ifstream wasm_file{fs::path{json_file_path}.replace_filename(filename)};

    return fizzy::bytes(
        std::istreambuf_iterator<char>{wasm_file}, std::istreambuf_iterator<char>{});
}

struct test_settings
{
    bool skip_validation = false;
};

struct test_results
{
    int passed = 0;
    int failed = 0;
    int skipped = 0;
};

class test_runner
{
public:
    explicit test_runner(const test_settings& ts) : settings{ts} {}

    test_results run_from_file(const fs::path& path)
    {
        log("Running tests from " + path.string());

        std::ifstream test_file{path};
        const json j = json::parse(test_file);

        for (const auto& cmd : j.at("commands"))
        {
            const auto type = cmd.at("type").get<std::string>();

            log_no_newline("Line " + std::to_string(cmd.at("line").get<int>()) + ": " + type + " ");

            if (type == "module")
            {
                const auto filename = cmd.at("filename").get<std::string>();
                log_no_newline("Instantiating " + filename + " ");

                const std::string name =
                    (cmd.find("name") != cmd.end() ? cmd.at("name").get<std::string>() :
                                                     UnnamedModule);

                const auto wasm_binary = load_wasm_file(path, filename);
                try
                {
                    // TODO provide dummy imports if needed
                    instances[name] = fizzy::instantiate(fizzy::parse(wasm_binary));
                }
                catch (const fizzy::parser_error& ex)
                {
                    fail(std::string{"Parsing failed with error: "} + ex.what());
                    instances.erase(name);
                    continue;
                }
                catch (const fizzy::instantiate_error& ex)
                {
                    fail(std::string{"Instantiation failed with error: "} + ex.what());
                    instances.erase(name);
                    continue;
                }
                pass();
            }
            else if (type == "assert_return" || type == "action")
            {
                const auto& action = cmd.at("action");
                const auto action_type = action.at("type").get<std::string>();
                if (action_type == "invoke")
                {
                    auto result = invoke(action);
                    if (!result.has_value())
                        continue;

                    if (result->trapped)
                    {
                        fail("Function trapped.");
                        continue;
                    }

                    const auto& expected = cmd.at("expected");
                    if (expected.empty())
                    {
                        if (!result->result.has_value())
                            pass();
                        else
                            fail("Unexpected returned value.");
                        continue;
                    }

                    if (!result->result.has_value())
                    {
                        fail("More than 1 value returned.");
                        continue;
                    }

                    const auto expected_type = expected.at(0).at("type").get<std::string>();
                    uint64_t expected_value;
                    if (expected_type == "i32")
                        expected_value = json_to_value<int32_t>(expected.at(0).at("value"));
                    else if (expected_type == "i64")
                        expected_value = json_to_value<int64_t>(expected.at(0).at("value"));
                    else
                    {
                        skip("Unsupported expected type '" + expected_type + "'.");
                        continue;
                    }

                    const uint64_t actual_value = *result->result;
                    if (expected_value != actual_value)
                    {
                        std::stringstream message;
                        message << "Incorrect returned value. Expected: " << expected_value
                                << " (0x" << std::hex << expected_value << ") Actual: " << std::dec
                                << actual_value << " (0x" << std::hex << actual_value << std::dec
                                << ")";
                        fail(message.str());
                        continue;
                    }

                    pass();
                }
                else
                    skip("Unsupported action type '" + action_type + "'");
            }
            else if (type == "assert_trap")
            {
                const auto& action = cmd.at("action");
                const auto action_type = action.at("type").get<std::string>();
                if (action_type != "invoke")
                {
                    skip("Unsupported action type '" + action_type + "'");
                    continue;
                }

                auto result = invoke(action);
                if (!result.has_value())
                    continue;

                if (!result->trapped)
                {
                    fail("Function expected to trap, but it didn't.");
                    continue;
                }

                pass();
            }
            else if (type == "assert_invalid" || type == "assert_malformed")
            {
                // NOTE: assert_malformed should result in a parser error and
                //       assert_invalid should result in a validation error
                if (type == "assert_invalid" && settings.skip_validation)
                {
                    skip("Validation tests disabled.");
                    continue;
                }

                const auto module_type = cmd.at("module_type").get<std::string>();
                if (module_type != "binary")
                {
                    skip("Only binary modules are supported.");
                    continue;
                }

                const auto filename = cmd.at("filename").get<std::string>();
                const auto wasm_binary = load_wasm_file(path, filename);
                try
                {
                    fizzy::parse(wasm_binary);
                }
                catch (fizzy::parser_error const&)
                {
                    pass();
                    continue;
                }

                fail("Invalid module parsed successfully. Expected error: " +
                     cmd.at("text").get<std::string>());
            }
            else
                skip("Unsupported command type");
        }

        log(std::to_string(results.passed + results.failed + results.skipped) + " tests ran from " +
            path.filename().string() + ".\n  PASSED " + std::to_string(results.passed) +
            ", FAILED " + std::to_string(results.failed) + ", SKIPPED " +
            std::to_string(results.skipped) + ".\n");

        return results;
    }

private:
    std::optional<fizzy::execution_result> invoke(const json& action)
    {
        const auto module_name =
            (action.find("module") != action.end() ? action["module"] : UnnamedModule);

        const auto it_instance = instances.find(module_name);
        if (it_instance == instances.end())
        {
            skip("No instantiated module.");
            return std::nullopt;
        }

        auto& instance = it_instance->second;

        const auto func_name = action.at("field").get<std::string>();
        const auto func_idx = fizzy::find_exported_function(instance.module, func_name);
        if (!func_idx.has_value())
        {
            skip("Function '" + func_name + "' not found.");
            return std::nullopt;
        }

        std::vector<uint64_t> args;
        for (const auto& arg : action.at("args"))
        {
            const auto arg_type = arg.at("type").get<std::string>();
            uint64_t arg_value;
            if (arg_type == "i32")
                arg_value = json_to_value<int32_t>(arg.at("value"));
            else if (arg_type == "i64")
                arg_value = json_to_value<int64_t>(arg.at("value"));
            else
            {
                skip("Unsupported argument type '" + arg_type + "'.");
                return std::nullopt;
            }
            args.push_back(arg_value);
        }

        return fizzy::execute(instance, *func_idx, std::move(args));
    }

    void pass()
    {
        ++results.passed;
        std::cout << "PASSED\n";
    }

    void fail(std::string_view message)
    {
        ++results.failed;
        std::cout << "FAILED " << message << "\n";
    }

    void skip(std::string_view message)
    {
        ++results.skipped;
        std::cout << "SKIPPED " << message << "\n";
    }

    void log(std::string_view message) const { std::cout << message << "\n"; }

    void log_no_newline(std::string_view message) const { std::cout << message << std::flush; }

    test_settings settings;
    std::unordered_map<std::string, fizzy::Instance> instances;
    test_results results;
};

bool run_tests_from_dir(const fs::path& path, const test_settings& settings)
{
    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator{path})
    {
        if (e.is_regular_file() && e.path().extension() == JsonExtension)
            files.emplace_back(e);
    }

    std::sort(std::begin(files), std::end(files));

    test_results total;
    bool exception_thrown = false;
    for (const auto& f : files)
    {
        try
        {
            const auto res = test_runner{settings}.run_from_file(f);

            total.passed += res.passed;
            total.failed += res.failed;
            total.skipped += res.skipped;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Exception: " << ex.what() << "\n\n";
            exception_thrown = true;
        }
    }

    std::cout << "TOTAL " << (total.passed + total.failed + total.skipped) << " tests ran from "
              << path << ".\n  PASSED " << total.passed << ", FAILED " << total.failed
              << ", SKIPPED " << total.skipped << ".\n";

    return (total.failed == 0 && !exception_thrown);
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string dir;
        test_settings settings;

        for (auto i = 1; i < argc; ++i)
        {
            if (argv[i][0] == '-')
            {
                if (argv[i] == std::string{"--skip-validation"})
                    settings.skip_validation = true;
                else
                {
                    std::cerr << "Unknown argument: " << argv[i] << "\n";
                    return -1;
                }
            }
            else
                dir = argv[i];
        }

        if (dir.empty())
        {
            std::cerr << "Missing DIR argument\n";
            return -1;
        }

        const bool res = run_tests_from_dir(dir, settings);
        return res ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return -2;
    }
}
