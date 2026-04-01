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

add_requires("wxwidgets")
add_requires("spdlog", {configs = {header_only = true}})

target("RemotePlayWhatever", function()
    set_kind("binary")
    add_deps("OpenSteamAPI")
    add_defines("STEAMWORKS_CLIENT_INTERFACES")
    add_packages("wxwidgets", "spdlog")

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
