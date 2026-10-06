-- Game-free unit tests for src/Core. Build: xmake build CoreTests ; run: xmake run CoreTests
target("CoreTests", function()
    set_kind("binary")
    set_default(false)
    add_packages("nlohmann_json")
    add_files("../src/Core/*.cpp", "core_tests.cpp")
    add_includedirs("../src")
end)
