---
curl = reaper.Curl_EasyInit()
reaper.Curl_EasySetopt(curl, "URL", "https://www.landoleet.org/old/gfx_test.lua")

-- downloads url as file "reacurl-test"
retval, buf = reaper.Curl_EasyPerform(curl, nil, "reacurl-test")

-- gets url into memory
retval, buf = reaper.Curl_EasyPerform(curl)

reaper.Curl_EasyCleanup(curl) -- always cleanup
reaper.ShowConsoleMsg(buf)
