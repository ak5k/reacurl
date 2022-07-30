---
curl = reaper.Curl_EasyInit()
reaper.Curl_EasySetopt(curl, "URL", "https://www.landoleet.org/old/gfx_test.lua")

-- downloads url as file "reacurl-test"
retval = reaper.Curl_EasyPerform(curl, "reacurl-test", 1)

-- gets url into memory buf
retval, buf = reaper.Curl_EasyPerform(curl)

reaper.Curl_EasyCleanup(curl) -- always cleanup
reaper.ShowConsoleMsg(buf)
