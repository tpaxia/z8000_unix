-- Capture the board console and exercise the ordinary V7 shell via its keyboard.
local output = ''
local sent = false
local booted = false
local matched_at
local settle = tonumber(os.getenv('Z8001UNIX_SETTLE') or '0')
local log = assert(io.open(os.getenv('Z8001UNIX_LOG') or '/tmp/z8001unix-console.log', 'w'))
manager.machine.natkeyboard.in_use = true
do
    local space = manager.machine.devices[':maincpu'].spaces['io_std']
    console_tap = space:install_write_tap(0xf0, 0xf1, 'unix_console', function(offset, data, mask)
        if (mask & 0xff00) ~= 0 then
            local c = string.char((data >> 8) & 255)
            output = output .. c
            log:write(c)
            log:flush()
        end
    end)
end
emu.register_frame_done(function()
    if not booted and output:find(': ', 1, true) then
        booted = true
        manager.machine.natkeyboard:post(os.getenv('Z8001UNIX_BOOT') or '\n')
    end
    if not sent and manager.machine.natkeyboard.empty and output:find('# ', 1, true) then
        sent = true
        manager.machine.natkeyboard:post(os.getenv('Z8001UNIX_INPUT') or 'echo hello | cat\nexit\n')
    end
    if output:gsub('\r', ''):find(os.getenv('Z8001UNIX_EXPECT') or '\nhello\n', 1, true) then
        if not matched_at then matched_at = emu.time() end
        if emu.time() - matched_at >= settle then
            print('Z8001UNIX TEST PASS')
            manager.machine:exit()
        end
    end
end)
