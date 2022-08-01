# ReaCurl
## libcurl Easy Interface partial bindings for REAPER ReaScript
```
Curl_EasyCleanup      Curl_EasyUnescape     Curl_MimeName     	    
Curl_EasyDuphandle    Curl_EasyUpkeep       Curl_MimeSubparts     	   
Curl_EasyEscape       Curl_Getdate          Curl_MimeType     	    
Curl_EasyInit         Curl_MimeAddpart      Curl_SlistAppend     	  
Curl_EasyPause        Curl_MimeData         Curl_SlistFreeAll     	  
Curl_EasyPerform      Curl_MimeFiledata     Curl_Version     	   
Curl_EasyReset        Curl_MimeFree         ...     	   
Curl_EasySetopt       Curl_MimeHeaders     	     	  
Curl_EasyStrerror     Curl_MimeInit     	    
```
See [examples](https://curl.se/libcurl/c/example.html), [curl](https://www.github.com/curl/curl) docs/examples and include/curl/curl.h, and [ReaCurl example](https://github.com/ak5k/reacurl/blob/main/examples/reacurl-get.lua). Pass options for `Curl_EasySetopt` as string, without `CURLOPT_` prefix, e.g. `CURLOPT_URL` as `"URL"`. Pass e.g. `curl_slist` datatype as `curl_data_type` with empty string or `0`/`nil`. ReaCurl uses internal callbacks for `{READ,WRITE}{DATA,FUNCTION}`; no need to set them. `Curl_EasyPerform` string parameter `buf` will be used as `{READ,WRITE}{DATA}` setting. If boolean `isPathIn` is set, string `buf` will be interpreted as filepath.