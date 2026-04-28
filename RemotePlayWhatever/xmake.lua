local OSW_ROOT    = path.join(os.scriptdir(), "../open-steamworks")
local OSW_INCLUDE = path.join(OSW_ROOT, "OpenSteamworks")
local OSW_SRC     = path.join(OSW_ROOT, "OpenSteamAPI/src")

target("OpenSteamAPI", function()
    set_kind("shared")
    add_includedirs(OSW_INCLUDE, OSW_SRC, { public = true })
    add_defines("STEAM_API_EXPORTS", "STEAMWORKS_CLIENT_INTERFACES")

	set_pcxxheader(path.join(OSW_INCLUDE, "SteamClient.h")) -- Precompiled header
    add_files(
        path.join(OSW_SRC, "CCallbackMgr.cpp"),
        path.join(OSW_SRC, "ClientWrap.cpp"),
        path.join(OSW_SRC, "Interface_OSW.cpp")
    )

    if is_os("windows") then
        add_syslinks("advapi32", "ole32", "oleaut32")
        if is_arch("x64") then
            set_basename(is_mode("debug") and "OpenSteamAPI64d" or "OpenSteamAPI64")
        else
            set_basename(is_mode("debug") and "OpenSteamAPId" or "OpenSteamAPI")
        end
    elseif is_os("linux") then
        add_syslinks("dl")
    elseif is_os("macosx") then
        add_files(path.join(OSW_SRC, "OSXPathHelper.mm"))
        add_frameworks("Foundation")
    end
end)

package("wxwidgets_static", function()
    add_deps("cmake")
    set_sourcedir(path.join(os.projectdir(), "third_party/wxwidgets"))

    on_install(function (package)
        local configs = {}

        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))

        table.insert(configs, "-DwxBUILD_SHARED=" .. (package:config("shared") and "ON" or "OFF"))
        table.insert(configs, "-DwxBUILD_MONOLITHIC=ON")
        table.insert(configs, "-DwxBUILD_TESTS=OFF")
        table.insert(configs, "-DwxBUILD_SAMPLES=OFF")

        if package:is_plat("windows") then
            table.insert(configs, "-DwxMSW=ON")
        end

        import("package.tools.cmake").install(package, configs)

        package:add("includedirs", "include")
        package:add("includedirs", "include/msvc")

        -- HACK:
        -- add_linkdirs() didn't take effect reliably (cause unknown)
        -- wxWidgets CMake output places libs in a different structure than expected,
        -- so we flatten vc_x64_lib into lib/ to ensure the linker can find them.
        local libdir = path.join(package:installdir(), "lib")
        local subdir = path.join(libdir, "vc_x64_lib")

        if os.isdir(subdir) then
            os.mv(path.join(subdir, "*.lib"), libdir)
        end
    end)

    on_load(function (package)
        package:add("defines", "UNICODE", "_UNICODE") -- Avoid `wxWidgets requires Unicode.`
    end)
end)
add_requires("wxwidgets_static")
add_requires("spdlog", {configs = {header_only = true}})

target("RemotePlayWhatever", function()
    set_kind("binary")
    add_deps("OpenSteamAPI")
    add_defines("STEAMWORKS_CLIENT_INTERFACES")
    add_defines("wxMONOLITHIC", "wxNO_GL_LIB")
    add_packages("wxwidgets_static", "spdlog")

    add_files("./*.cpp")

    if is_os("windows") then
        add_files("appresource.rc")
    elseif is_os("linux") then
        set_basename("remoteplaywhatever")
        add_syslinks("dl")
        on_install(function(target)
            os.cp(target:targetfile(), "/usr/bin/remoteplaywhatever")
        end)
    elseif is_os("macosx") then
        add_frameworks("Foundation")
    end
end)
