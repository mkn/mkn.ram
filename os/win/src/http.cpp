/**
Copyright (c) 2013, Philip Deegan.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Philip Deegan nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "kul/def.hpp"
#include "kul/log.hpp"
#include "kul/http.hpp"

void kul::http::Server::start() KTHROW(kul::http::Exception){
    ULONG RequestBufferLength     = sizeof(HTTP_REQUEST) + 2048;
    PCHAR pRequestBuffer         = (PCHAR) wAlloc(RequestBufferLength);
    if(pRequestBuffer == NULL)
        KEXCEPT(Exception, "Buffer allocation failed: " + std::to_string(ERROR_NOT_ENOUGH_MEMORY));

    PHTTP_REQUEST req = (PHTTP_REQUEST) pRequestBuffer;
    HTTP_REQUEST_ID requestId;
    HTTP_SET_NULL_ID(&requestId);
    DWORD bytesRead;
    ULONG r = 0;
    while(1){
        RtlZeroMemory(req, RequestBufferLength);
        r = HttpReceiveHttpRequest(this->q, requestId, 0, req, RequestBufferLength, &bytesRead, NULL);
        if(r == NO_ERROR){
            if(req->Verb == HttpVerbGET){
                get(req);
            }else
            if(req->Verb == HttpVerbPOST){
                post(req);
            }else
            if(req->Verb == HttpVerbHEAD){
                // head(req);
            }else
                KLOG(INF) << "Unrecognised http method type";
        }else
        if(r == ERROR_MORE_DATA){
            requestId = req->RequestId;
            RequestBufferLength = bytesRead;
            wFreeM(pRequestBuffer);
            pRequestBuffer = (PCHAR) wAlloc(RequestBufferLength);
        }else if(r == ERROR_CONNECTION_INVALID && !HTTP_IS_NULL_ID(&requestId)){
            HTTP_SET_NULL_ID(&requestId);
        }else KEXCEPT(Exception, "HttpReceiveHttpRequest failed: " + std::to_string(r));
    }
}
void kul::http::Server::stop(){
    ULONG r = 0;
    HttpShutdownRequestQueue(q);
    if(r != NO_ERROR) KEXCEPT(Exception, "HttpShutdownRequestQueue failed: " + std::to_string(r));
    if(q) CloseHandle(q);
}

void kul::http::Server::initialiseReponse(HTTP_RESPONSE& response, const uint16_t& status, PSTR reason){
    RtlZeroMemory(&response, sizeof(response));
    response.StatusCode        = 200;
    response.pReason        = "OK";
    response.ReasonLength    = strlen(response.pReason);
}
void kul::http::Server::addKnownHeader(HTTP_RESPONSE& response, _HTTP_HEADER_ID headerID, PSTR rawValue){
    response.Headers.KnownHeaders[HttpHeaderContentType].pRawValue = "text/html";
    response.Headers.KnownHeaders[HttpHeaderContentType].RawValueLength = (USHORT) strlen("text/html");
}
bool kul::http::Server::get(PHTTP_REQUEST req){
    HTTP_RESPONSE response;
    initialiseReponse(response, 200, "OK");
    addKnownHeader(response, HttpHeaderContentType, "text/html");

    HTTP_DATA_CHUNK dataChunk;
    DWORD           result;
    DWORD           bytesSent;

    std::string s(req->pRawUrl);
    std::string a;
    if(s.find("?") != std::string::npos){
        a = s.substr(s.find("?") + 1);
        s = s.substr(0, s.find("?"));
    }
    const std::pair<kul::hash::set::String, std::string>& p(handle(s, asAttributes(a)));
    if(p.second.size()){
        dataChunk.DataChunkType             = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer         = (PVOID) p.second.c_str();
        dataChunk.FromMemory.BufferLength     = p.second.size();

        response.EntityChunkCount             = 1;
        response.pEntityChunks                 = &dataChunk;
    }
    result = HttpSendHttpResponse(this->q, req->RequestId, 0, &response, NULL, &bytesSent, NULL, 0, NULL, NULL); 
    if(result != NO_ERROR)
        KEXCEPT(Exception, "HttpSendHttpResponse failed with: " + std::to_string(result));

    return result;
}
bool kul::http::Server::post(PHTTP_REQUEST req){
    HTTP_RESPONSE     response;
    DWORD             result;
    DWORD             bytesSent;
    ULONG             EntityBufferLength = 512;
    PUCHAR             rstr = (PUCHAR) wAlloc(EntityBufferLength);
    ULONG             bytes = 0;
    CHAR             szContentLength[MAX_ULONG_STR];
    HTTP_DATA_CHUNK dataChunk;

    if (rstr == NULL){
        postClean(rstr);
        KEXCEPT(Exception, "Insufficient resources " + std::to_string(ERROR_NOT_ENOUGH_MEMORY));
    }

    initialiseReponse(response, 200, "OK");
    if(req->Flags & HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS){
        std::string atts;
        do{
            bytes = 0;
            result = HttpReceiveRequestEntityBody(this->q, req->RequestId, 0, rstr, EntityBufferLength, &bytes, NULL);
            switch(result){
                case NO_ERROR:
                    for(ULONG i = 0; i < bytes; i++) atts += rstr[i];
                    break;
                case ERROR_HANDLE_EOF:
                {
                    for(ULONG i = 0; i < bytes; i++) atts += rstr[i];
                    sprintf_s(szContentLength, MAX_ULONG_STR, "%lu", bytes);
                    addKnownHeader(response, HttpHeaderContentLength, szContentLength);
                    result = HttpSendHttpResponse(this->q, req->RequestId,
                                HTTP_SEND_RESPONSE_FLAG_MORE_DATA, &response,
                                NULL, &bytesSent, NULL, 0, NULL, NULL);
                    if(result != NO_ERROR){
                        postClean(rstr);
                        KEXCEPT(Exception, "HttpSendHttpResponse failed with: " + std::to_string(result));
                    }
                    const std::pair<kul::hash::set::String, std::string>& p(handle(req->pRawUrl, asAttributes(atts)));
                    dataChunk.DataChunkType             = HttpDataChunkFromMemory;
                    dataChunk.FromMemory.pBuffer         = (PVOID) p.second.c_str();
                    dataChunk.FromMemory.BufferLength     = p.second.size();
                    result = HttpSendResponseEntityBody(this->q, req->RequestId, 0, 1, &dataChunk, NULL, NULL, 0, NULL, NULL);
                    if(result != NO_ERROR){
                        postClean(rstr);
                        KEXCEPT(Exception, "HttpSendResponseEntityBody failed with: " + std::to_string(result));
                    }
                    break;
                }
                default:
                    postClean(rstr);
                    return result;
            }
        }while(TRUE);
    }else{ // This request does not have an entity body.
        result = HttpSendHttpResponse(this->q, req->RequestId, 0, &response, NULL, &bytesSent, NULL,0, NULL, NULL);
        if(result != NO_ERROR){
            postClean(rstr);
            KEXCEPT(Exception, "HttpSendHttpResponse failed with: " + std::to_string(result));
        }
    }
    postClean(rstr);
    return result;
}
void kul::http::Server::postClean(PUCHAR rstr){
    if(rstr) wFreeM(rstr);
}

void kul::http::_1_1GetRequest::send(const std::string& h, const std::string& res, const uint16_t& p){
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) KEXCEPT(Exception, "WSAStartup failed");
    SOCKET sock = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    struct hostent *host;
    host = gethostbyname(h.c_str());
    SOCKADDR_IN SockAddr;
    SockAddr.sin_port = htons(p);
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr);
    if(connect(sock,(SOCKADDR*)(&SockAddr),sizeof(SockAddr)) != 0)
        KEXCEPT(Exception, "Could not connect");
    std::string req(toString(h, res));
    ::send(sock, req.c_str(), req.size(), 0);
    char buffer[10000];
    int nDataLength;
    std::string s;
    while((nDataLength = recv(sock,buffer,10000,0)) > 0){
        int i = 0;
        while (buffer[i] >= 32 || buffer[i] == '\n' || buffer[i] == '\r') {
            s += buffer[i];//do something with buffer[i]
            i += 1;
        }
        memset(&buffer[0], 0, sizeof(buffer));
    }
    handle(s);
    closesocket(sock);
    WSACleanup();
}

void kul::http::_1_1PostRequest::send(const std::string& h, const std::string& res, const uint16_t& p){
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) KEXCEPT(Exception, "WSAStartup failed");
    SOCKET sock = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    struct hostent *host;
    host = gethostbyname(h.c_str());
    SOCKADDR_IN SockAddr;
    SockAddr.sin_port = htons(p);
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr);
    if(connect(sock,(SOCKADDR*)(&SockAddr),sizeof(SockAddr)) != 0)
        KEXCEPT(Exception, "Could not connect");
    std::string req(toString(h, res));
    ::send(sock, req.c_str(), req.size(), 0);
    char buffer[10000];
    int16_t nDataLength;
    std::string s;
    while((nDataLength = recv(sock, buffer, 10000, 0)) > 0){
        uint16_t i = 0;
        while (buffer[i] >= 32 || buffer[i] == '\n' || buffer[i] == '\r') {
            s += buffer[i];//do something with buffer[i]
            i += 1;
        }
        memset(&buffer[0], 0, sizeof(buffer));
    }
    handle(s);
    closesocket(sock);
    WSACleanup();
}
