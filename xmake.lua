add_rules("mode.debug", "mode.release")

set_runtimes("MT")
includes("./RemotePlayWhatever/")

-- Platform detection
if is_os("windows") then
    SYSTEM_OS = "Win"
elseif is_os("linux") then
    SYSTEM_OS = "Linux"
else
    raise("Unsupported platform!")
end
