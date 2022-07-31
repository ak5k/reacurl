---
curl = reaper.Curl_EasyInit()
reaper.Curl_EasySetopt(curl, "URL", "https://www.landoleet.org/old/gfx_test.lua")

-- downloads url as file "reacurl-test"
retval, buf = reaper.Curl_EasyPerform(curl)
reaper.ShowConsoleMsg(buf)

-- gets url into memory buf
retval, buf = reaper.Curl_EasyPerform(curl,reaper.GetResourcePath() .. "/reacurl-test",true)

reaper.Curl_EasyCleanup(curl) -- always cleanup
reaper.ShowConsoleMsg("\nalso downloaded as " .. buf)
