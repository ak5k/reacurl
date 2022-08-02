#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
#include <reaper_vararg.hpp>
#include <string>

#define REAPERAPI_IMPLEMENT
#include <reaper_plugin_functions.h>

namespace reacurl {

struct Memory {
    const char* read;
    char* write;
    size_t sizeread;
    size_t sizewrite;
    size_t sizealloc;
};

static size_t read_callback(char* ptr, size_t size, size_t nmemb, void* userp)
{
    size_t len = size * nmemb;
    struct Memory* mem = (struct Memory*)userp;

    if (len < 1) {
        return 0;
    }

    if (mem->sizeread) {
        if (len > mem->sizeread) {
            len = mem->sizeread;
        }
        memcpy(ptr, mem->read, len);
        mem->read += len;
        mem->sizeread -= len;
        return len;
    }

    return 0; /* no more data left to deliver */
}

static size_t write_callback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp)
{
    size_t len = size * nmemb;
    struct Memory* mem = (struct Memory*)userp;

    if (mem->sizealloc < len) {
        char* ptr = (char*)realloc(mem->write, len * 2);
        if (!ptr) {
            return 0;
        }
        mem->sizealloc = len * 2;
        mem->write = ptr;
        // if (!realloc_cmd_ptr(
        //         &(mem->write),
        //         (int*)&(mem->sizealloc),
        //         (int)(len * 2))) {
        //     return 0;
        // }
    }

    if (mem->sizealloc < mem->sizewrite + len + 1) {
        char* ptr = (char*)realloc(mem->write, mem->sizealloc * 2);
        if (!ptr) {
            return 0;
        }
        mem->sizealloc = mem->sizealloc * 2;
        mem->write = ptr;
        // if (!realloc_cmd_ptr(
        //         &(mem->write),
        //         (int*)&(mem->sizealloc),
        //         (int)(mem->sizealloc * 2))) {
        //     return 0;
        // }
    }

    memcpy(&(mem->write[mem->sizewrite]), contents, len);
    mem->sizewrite += len;
    mem->write[mem->sizewrite] = 0;

    return len;
}

const char* defCurl_EasyInit =
    "CURL*\0\0\0returns curl handle\n"
    "study include/curl/curl.h"
    "and curl/docs/examples"
    "at github.com/curl/curl\n";

CURL* Curl_EasyInit()
{
    static int res = (int)curl_global_init(CURL_GLOBAL_DEFAULT);
    (void)res;
    return curl_easy_init();
}

const char* defCurl_EasyCleanup =
    "void\0CURL*\0curl\0always cleanup curl handle when finished\n";

void Curl_EasyCleanup(CURL* curl)
{
    return curl_easy_cleanup(curl);
}

const char* defCurl_EasySetopt =
    "int\0CURL*,const char*,const char*,void*"
    "\0curl,option,value,curl_type_dataOptional\0"
    "use option without CURLOPT_ prefix\n"
    "set value as string\n"
    "slist and blob are optional\n"
    "EasyPerform always uses internal memory and file read and write "
    "callbacks\n"
    "returns CURLcode\n";

int Curl_EasySetopt(
    CURL* curl,
    const char* option,
    const char* value,
    void* curl_type_dataOptional)
{
    auto curl_easyoption = curl_easy_option_by_name(option);
    auto option_type = curl_easyoption->type;
    if (option_type == CURLOT_LONG || option_type == CURLOT_VALUES ||
        option_type == CURLOT_OFF_T) {
        return curl_easy_setopt(curl, curl_easyoption->id, std::stoi(value));
    }
    if (option_type == CURLOT_STRING) {
        return curl_easy_setopt(curl, curl_easyoption->id, value);
    }
    if (option_type == CURLOT_BLOB) {
        return curl_easy_setopt(
            curl,
            curl_easyoption->id,
            (curl_blob*)curl_type_dataOptional);
    }
    if (option_type == CURLOT_SLIST) {
        return curl_easy_setopt(
            curl,
            curl_easyoption->id,
            (curl_slist*)curl_type_dataOptional);
    }
    return (int)curl_easy_setopt(curl, curl_easyoption->id, nullptr);
}

const char* defCurl_EasyPerform =
    "int\0CURL*,char*,int,bool*\0curl,"
    "bufOutOptionalNeedBig,"
    "bufOutOptionalNeedBig_sz,isPathInOptional\0"
    "runs/executes/performs curl\n"
    "set isPathIn true to file download/upload\n"
    "EasyPerform always uses internal memory and file read and write "
    "callbacks\n"
    "returns CURLcode\n";

int Curl_EasyPerform(
    CURL* curl,
    char* bufInOutOptionalNeedBig,
    int bufInOutOptionalNeedBig_sz,
    bool* isPathInOptional)
{
    bool isBuf {true};
    bool isFile {false};

    struct Memory data;
    FILE* fp;

    if (isPathInOptional != nullptr && isPathInOptional) {
        isBuf = false;
        isFile = true;
    }

    if (isBuf) {
        data.write = nullptr;
        data.sizealloc = 0;
        data.sizewrite = 0;
        if (bufInOutOptionalNeedBig != nullptr &&
            strlen(bufInOutOptionalNeedBig) != 0) {
            data.read = bufInOutOptionalNeedBig;
            data.sizeread = strlen(bufInOutOptionalNeedBig);
            curl_easy_setopt(curl, CURLOPT_READDATA, &data);
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
            curl_easy_setopt(
                curl,
                CURLOPT_INFILESIZE_LARGE,
                (curl_off_t)data.sizeread);
        }
        else {
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        }
    }

    if (isFile) {
        fp = fopen(bufInOutOptionalNeedBig, "a+");
        if (!fp) {
            return 1;
        }
        struct stat file_info;
        if (fstat(fileno(fp), &file_info) != 0) {
            return 1; /* cannot continue */
        }
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, fread);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
        curl_easy_setopt(curl, CURLOPT_READDATA, fp);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(
            curl,
            CURLOPT_INFILESIZE_LARGE,
            (curl_off_t)file_info.st_size);
    }

    auto res = curl_easy_perform(curl);

    if (isFile) {
        fclose(fp);
    }
    if (isBuf) {
        if (data.sizewrite > 0 && realloc_cmd_ptr(
                                      &bufInOutOptionalNeedBig,
                                      &bufInOutOptionalNeedBig_sz,
                                      (int)data.sizewrite)) {
            memcpy(bufInOutOptionalNeedBig, data.write, data.sizewrite);
        }
        free(data.write);
        // char* ptr = bufInOutOptionalNeedBig;
        // bufInOutOptionalNeedBig = data.write;
        // (void)bufInOutOptionalNeedBig;
        // FreeHeapPtr(ptr);
    }
    return (int)res;
}

const char* defCurl_EasyStrerror =
    "const char*\0int\0curlcode\0"
    "converts CURLcode integer to string";

const char* Curl_EasyStrerror(int curlcode)
{
    return curl_easy_strerror((CURLcode)curlcode);
}

const char* defCurl_EasyDuphandle =
    "CURL*\0CURL*\0curl\0"
    "duplicates curl handle\n";

CURL* Curl_EasyDuphandle(CURL* curl)
{
    return curl_easy_duphandle(curl);
}

const char* defCurl_EasyReset =
    "void\0CURL*\0curl\0"
    "resets curl handle\n";

void Curl_EasyReset(CURL* curl)
{
    return curl_easy_reset(curl);
}

const char* defCurl_EasyUpkeep =
    "int\0CURL*\0curl\0"
    "connection upkeep for curl handle\n"
    "returns CURLcode\n";

int Curl_EasyUpkeep(CURL* curl)
{
    return (int)curl_easy_upkeep(curl);
}

const char* defCurl_Version =
    "const char*\0\0\0"
    "returns curl version\n";

const char* Curl_Version()
{
    return curl_version();
}

const char* defCurl_Getdate =
    "int\0const char*\0time_string\0"
    "returns time in seconds since 1 Jan 1970 of time_string\n";

int Curl_Getdate(const char* time_string)
{
    return (int)curl_getdate(time_string, nullptr);
}

const char* defCurl_EasyEscape =
    "const char*\0CURL*,const char*,int\0curl,string,length\0"
    "escapes URL strings\n";

const char* Curl_EasyEscape(CURL* curl, const char* string, int length)
{
    return curl_easy_escape(curl, string, length);
}

const char* defCurl_EasyUnescape =
    "const char*\0CURL*,const char*,int,int\0curl,string,length,outlength\0"
    "unescapes URL strings\n";

const char* Curl_EasyUnescape(
    CURL* curl,
    const char* string,
    int length,
    int outlength)
{
    return curl_easy_unescape(curl, string, length, &outlength);
}

const char* defCurl_SlistAppend =
    "curl_slist*\0curl_slist*,const char*\0slist,string\0"
    "creates new/appends to linked string list\n"
    "pass nil/NULL slist to create new\n";

curl_slist* Curl_SlistAppend(curl_slist* slist, const char* string)
{
    return curl_slist_append(slist, string);
}

const char* defCurl_SlistFreeAll =
    "void\0curl_slist*\0slist\0"
    "free previously built curl_slist\n";

void Curl_SlistFreeAll(curl_slist* slist)
{
    return curl_slist_free_all(slist);
}

const char* defCurl_EasyPause =
    "int\0CURL*,int\0curl,bitmask\0"
    "pause/unpause transfers\n"
    "returns CURLcode\n";

int Curl_EasyPause(CURL* curl, int bitmask)
{
    return (int)curl_easy_pause(curl, bitmask);
}

const char* defCurl_MimeInit =
    "curl_mime*\0CURL*\0curl\0"
    "return curl_mime\n";

curl_mime* Curl_MimeInit(CURL* curl)
{
    return curl_mime_init(curl);
}

const char* defCurl_MimeAddpart =
    "curl_mimepart*\0curl_mime*\0mime\0"
    "append new empty part to mime\n";

curl_mimepart* Curl_MimeAddpart(curl_mime* mime)
{
    return curl_mime_addpart(mime);
}

const char* defCurl_MimeData =
    "int\0curl_mimepart*,const char*,const int*\0part,data,datasizeInOptional\0"
    "set mime part data source\n"
    "returns CURLcode\n";

int Curl_MimeData(
    curl_mimepart* part,
    const char* data,
    const int* datasizeInOptional)
{
    size_t datasize;
    if (datasizeInOptional == nullptr) {
        datasize = CURL_ZERO_TERMINATED;
    }
    else {
        datasize = *datasizeInOptional;
    }
    return curl_mime_data(part, data, datasize);
}

const char* defCurl_MimeType =
    "int\0curl_mimepart*,const char*\0part,mimetype\0"
    "set mime type\n"
    "returns CURLcode\n";

int Curl_MimeType(curl_mimepart* part, const char* mimetype)
{
    return curl_mime_type(part, mimetype);
}

const char* defCurl_MimeSubparts =
    "int\0curl_mimepart*,curl_mime*\0part,subparts\0"
    "set mime part data source from subparts\n"
    "returns CURLcode\n";

int Curl_MimeSubparts(curl_mimepart* part, curl_mime* subparts)
{
    return curl_mime_subparts(part, subparts);
}

const char* defCurl_MimeHeaders =
    "int\0curl_mimepart*,curl_slist*,int\0part,headers,take_ownership\0"
    "set mime part headers\n"
    "returns CURLcode\n";

int Curl_MimeHeaders(
    curl_mimepart* part,
    curl_slist* headers,
    int take_ownership)
{
    return curl_mime_headers(part, headers, take_ownership);
}

const char* defCurl_MimeFiledata =
    "int\0curl_mimepart*,const char*\0part,filename\0"
    "set mime part data source from named file\n"
    "returns CURLcode\n";

int Curl_MimeFiledata(curl_mimepart* part, const char* filename)
{
    return curl_mime_filedata(part, filename);
}

const char* defCurl_MimeFree =
    "void\0curl_mime*\0mime\0"
    "release mime handle\n";

void Curl_MimeFree(curl_mime* mime)
{
    return curl_mime_free(mime);
}

const char* defCurl_MimeName =
    "int\0curl_mimepart*,const char*\0part,name\0"
    "release mime handle\n";

int Curl_MimeName(curl_mimepart* part, const char* name)
{
    return curl_mime_name(part, name);
}

const char* defCurl_EasySend =
    "int\0CURL*,const char*,int,int*\0curl,buffer,buflen,nOut\0"
    "easy send\n"
    "returns CURLcode and sent";

int Curl_EasySend(CURL* curl, const char* buffer, int buflen, int* nOut)
{
    size_t n;
    int res = curl_easy_send(curl, buffer, size_t(buflen), &n);
    *nOut = (int)n;
    return res;
}

const char* defCurl_EasyReceive =
    "int\0CURL*,int,int*,char*,int\0curl,buflen,nOut,bufOutNeedBig,"
    "bufOutNeedBig_sz\0"
    "easy receive\n"
    "returns CURLcode, received and buffer";

int Curl_EasyReceive(
    CURL* curl,
    int buflen,
    int* nOut,
    char* bufOutNeedBig,
    int bufOutNeedBig_sz)
{
    size_t n;
    int res = 1;
    char* buffer = (char*)malloc(buflen + 1);
    res = curl_easy_recv(curl, buffer, (buflen + 1) * sizeof(buffer), &n);
    *nOut = (int)n;
    if (!realloc_cmd_ptr(&bufOutNeedBig, &bufOutNeedBig_sz, *nOut)) {
        res = 1;
    }
    free(buffer);
    return res;
}

const char* defCurl_WaitOnSocket =
    "int\0CURL*,bool,int\0curl,for_receive,timeout_ms\0"
    "returns the number of signalled sockets or -1\n"
    "";

static int Curl_WaitOnSocket(CURL* curl, bool for_receive, int timeout_ms)
{
    curl_socket_t sockfd;
    if (curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd) != CURLE_OK) {
        return -1;
    };

    struct timeval tv;
    fd_set infd, outfd, errfd;
    int res;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    FD_ZERO(&infd);
    FD_ZERO(&outfd);
    FD_ZERO(&errfd);

    FD_SET(sockfd, &errfd); /* always check for error */

    if (for_receive) {
        FD_SET(sockfd, &infd);
    }
    else {
        FD_SET(sockfd, &outfd);
    }

    /* select() returns the number of signalled sockets or -1 */
    res = select((int)sockfd + 1, &infd, &outfd, &errfd, &tv);
    return res;
}

void Register()
{
    plugin_register("API_Curl_WaitOnSocket", (void*)Curl_WaitOnSocket);
    plugin_register("APIdef_Curl_WaitOnSocket", (void*)defCurl_WaitOnSocket);
    plugin_register(
        "APIvararg_Curl_WaitOnSocket",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_WaitOnSocket>));

    plugin_register("API_Curl_EasyReceive", (void*)Curl_EasyReceive);
    plugin_register("APIdef_Curl_EasyReceive", (void*)defCurl_EasyReceive);
    plugin_register(
        "APIvararg_Curl_EasyReceive",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyReceive>));

    plugin_register("API_Curl_EasySend", (void*)Curl_EasySend);
    plugin_register("APIdef_Curl_EasySend", (void*)defCurl_EasySend);
    plugin_register(
        "APIvararg_Curl_EasySend",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasySend>));

    plugin_register("API_Curl_EasySetopt", (void*)Curl_EasySetopt);
    plugin_register("APIdef_Curl_EasySetopt", (void*)defCurl_EasySetopt);
    plugin_register(
        "APIvararg_Curl_EasySetopt",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasySetopt>));

    plugin_register("API_Curl_EasyPerform", (void*)Curl_EasyPerform);
    plugin_register("APIdef_Curl_EasyPerform", (void*)defCurl_EasyPerform);
    plugin_register(
        "APIvararg_Curl_EasyPerform",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyPerform>));

    plugin_register("API_Curl_MimeName", (void*)Curl_MimeName);
    plugin_register("APIdef_Curl_MimeName", (void*)defCurl_MimeName);
    plugin_register(
        "APIvararg_Curl_MimeName",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeName>));

    plugin_register("API_Curl_MimeFree", (void*)Curl_MimeFree);
    plugin_register("APIdef_Curl_MimeFree", (void*)defCurl_MimeFree);
    plugin_register(
        "APIvararg_Curl_MimeFree",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeFree>));

    plugin_register("API_Curl_MimeFiledata", (void*)Curl_MimeFiledata);
    plugin_register("APIdef_Curl_MimeFiledata", (void*)defCurl_MimeFiledata);
    plugin_register(
        "APIvararg_Curl_MimeFiledata",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeFiledata>));

    plugin_register("API_Curl_MimeHeaders", (void*)Curl_MimeHeaders);
    plugin_register("APIdef_Curl_MimeHeaders", (void*)defCurl_MimeHeaders);
    plugin_register(
        "APIvararg_Curl_MimeHeaders",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeHeaders>));

    plugin_register("API_Curl_MimeSubparts", (void*)Curl_MimeSubparts);
    plugin_register("APIdef_Curl_MimeSubparts", (void*)defCurl_MimeSubparts);
    plugin_register(
        "APIvararg_Curl_MimeSubparts",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeSubparts>));

    plugin_register("API_Curl_MimeType", (void*)Curl_MimeType);
    plugin_register("APIdef_Curl_MimeType", (void*)defCurl_MimeType);
    plugin_register(
        "APIvararg_Curl_MimeType",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeType>));

    plugin_register("API_Curl_MimeData", (void*)Curl_MimeData);
    plugin_register("APIdef_Curl_MimeData", (void*)defCurl_MimeData);
    plugin_register(
        "APIvararg_Curl_MimeData",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeData>));

    plugin_register("API_Curl_MimeAddpart", (void*)Curl_MimeAddpart);
    plugin_register("APIdef_Curl_MimeAddpart", (void*)defCurl_MimeAddpart);
    plugin_register(
        "APIvararg_Curl_MimeAddpart",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeAddpart>));

    plugin_register("API_Curl_MimeInit", (void*)Curl_MimeInit);
    plugin_register("APIdef_Curl_MimeInit", (void*)defCurl_MimeInit);
    plugin_register(
        "APIvararg_Curl_MimeInit",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_MimeInit>));

    plugin_register("API_Curl_EasyPause", (void*)Curl_EasyPause);
    plugin_register("APIdef_Curl_EasyPause", (void*)defCurl_EasyPause);
    plugin_register(
        "APIvararg_Curl_EasyPause",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyPause>));

    plugin_register("API_Curl_SlistFreeAll", (void*)Curl_SlistFreeAll);
    plugin_register("APIdef_Curl_SlistFreeAll", (void*)defCurl_SlistFreeAll);
    plugin_register(
        "APIvararg_Curl_SlistFreeAll",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_SlistFreeAll>));

    plugin_register("API_Curl_SlistAppend", (void*)Curl_SlistAppend);
    plugin_register("APIdef_Curl_SlistAppend", (void*)defCurl_SlistAppend);
    plugin_register(
        "APIvararg_Curl_SlistAppend",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_SlistAppend>));

    plugin_register("API_Curl_EasyUnescape", (void*)Curl_EasyUnescape);
    plugin_register("APIdef_Curl_EasyUnescape", (void*)defCurl_EasyUnescape);
    plugin_register(
        "APIvararg_Curl_EasyUnescape",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyUnescape>));

    plugin_register("API_Curl_EasyEscape", (void*)Curl_EasyEscape);
    plugin_register("APIdef_Curl_EasyEscape", (void*)defCurl_EasyEscape);
    plugin_register(
        "APIvararg_Curl_EasyEscape",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyEscape>));

    plugin_register("API_Curl_Getdate", (void*)Curl_Getdate);
    plugin_register("APIdef_Curl_Getdate", (void*)defCurl_Getdate);
    plugin_register(
        "APIvararg_Curl_Getdate",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_Getdate>));

    plugin_register("API_Curl_Version", (void*)Curl_Version);
    plugin_register("APIdef_Curl_Version", (void*)defCurl_Version);
    plugin_register(
        "APIvararg_Curl_Version",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_Version>));

    plugin_register("API_Curl_EasyUpkeep", (void*)Curl_EasyUpkeep);
    plugin_register("APIdef_Curl_EasyUpkeep", (void*)defCurl_EasyUpkeep);
    plugin_register(
        "APIvararg_Curl_EasyUpkeep",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyUpkeep>));

    plugin_register("API_Curl_EasyReset", (void*)Curl_EasyReset);
    plugin_register("APIdef_Curl_EasyReset", (void*)defCurl_EasyReset);
    plugin_register(
        "APIvararg_Curl_EasyReset",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyReset>));

    plugin_register("API_Curl_EasyDuphandle", (void*)Curl_EasyDuphandle);
    plugin_register("APIdef_Curl_EasyDuphandle", (void*)defCurl_EasyDuphandle);
    plugin_register(
        "APIvararg_Curl_EasyDuphandle",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyDuphandle>));

    plugin_register("API_Curl_EasyStrerror", (void*)Curl_EasyStrerror);
    plugin_register("APIdef_Curl_EasyStrerror", (void*)defCurl_EasyStrerror);
    plugin_register(
        "APIvararg_Curl_EasyStrerror",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyStrerror>));

    plugin_register("API_Curl_EasyInit", (void*)Curl_EasyInit);
    plugin_register("APIdef_Curl_EasyInit", (void*)defCurl_EasyInit);
    plugin_register(
        "APIvararg_Curl_EasyInit",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyInit>));

    plugin_register("API_Curl_EasyCleanup", (void*)Curl_EasyCleanup);
    plugin_register("APIdef_Curl_EasyCleanup", (void*)defCurl_EasyCleanup);
    plugin_register(
        "APIvararg_Curl_EasyCleanup",
        reinterpret_cast<void*>(&InvokeReaScriptAPI<&Curl_EasyCleanup>));
}

} // namespace reacurl

extern "C" {
REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(
    REAPER_PLUGIN_HINSTANCE hInstance,
    reaper_plugin_info_t* rec)
{
    (void)hInstance;
    if (!rec) {
        curl_global_cleanup();
        return 0;
    }
    else if (
        rec->caller_version != REAPER_PLUGIN_VERSION ||
        REAPERAPI_LoadAPI(rec->GetFunc)) {
        return 0;
    }
    // curl_global_init(CURL_GLOBAL_ALL);
    reacurl::Register();
    return 1;
}
}