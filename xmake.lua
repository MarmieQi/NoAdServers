add_rules("mode.debug", "mode.release")

option("levimc_repo")
    set_default("https://github.com/LiteLDev/xmake-repo.git")
    set_showmenu(true)
    set_description("Set the levimc-repo path or url")
option_end()

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("server", "client")
option_end()

add_repositories("levimc-repo " .. (get_config("levimc_repo") or "https://github.com/LiteLDev/xmake-repo.git"))

add_requires("levilamina 26.60.0", {configs = {target_type = get_config("target_type") or "client"}})
add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

rule("noadservers.oreui.embed")
    before_build(function (target)
        os.execv("node", {path.join(os.projectdir(), "frontend", "build.mjs")})
        local js_path = path.join(os.projectdir(), "frontend", "dist", "no-ad-servers.js")
        local gen_dir = path.join(target:autogendir(), "include")
        os.mkdir(gen_dir)
        local data = io.readfile(js_path, {encoding = "binary"})
        local hex = {}
        for i = 1, #data do
            hex[i] = string.format("0x%02x", string.byte(data, i))
        end
        local header = string.format(
            "#pragma once\n#include <cstddef>\n#include <string_view>\nnamespace noadservers::oreui {\ninline constexpr unsigned char kEmbeddedFrontendBytes[] = {\n%s\n};\ninline std::string_view const kEmbeddedFrontendJs{\n    reinterpret_cast<char const*>(kEmbeddedFrontendBytes),\n    sizeof(kEmbeddedFrontendBytes)\n};\n}\n",
            table.concat(hex, ",")
        )
        local out_path = path.join(gen_dir, "EmbeddedFrontendJs.h")
        if not os.isfile(out_path) or io.readfile(out_path) ~= header then
            io.writefile(out_path, header)
        end
        target:add("includedirs", gen_dir)
    end)
rule_end()

target("NoAdServers")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker", {modVersion = "26.60.2"})
    add_rules("noadservers.oreui.embed")
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE", "_HAS_CXX23=1")
        set_exceptions("none") -- To avoid conflicts with /EHa.
        add_cxflags("/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end
    add_packages("levilamina")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
