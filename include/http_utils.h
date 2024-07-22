#pragma once
#include <unordered_map>
#include <string>

#define EN "en"
#define FR "fr"

enum LANGUAGE_TYPE {
    LT_EN = 0,
    LT_FR
};

namespace HTTP_UTILS {
    enum HTTPCODE{
        // 200（OK）
        // 请求已成功，请求所希望的响应头或数据体将随此响应返回。
        OK = 200,

        // 304 Not Modified
        // 根据ETag标志，判断客户端请求的资源和服务器中当前的资源有没有发生变化
        // 如果没有发生变化，那么客户端可以继续使用之前请求的资源，即缓存可以继续使用
        // 如果服务器端生成ETag，发现和客户端If-None-Match字段的ETag值不同
        // 那么服务器端就需要将新的资源发送给客户端，即客户端需要更新自己的缓存了
        /*
            这个过程叫做 缓存验证过程
            第一次： 客户端请求某个资源。
                    服务器返回该资源的内容，并在响应头中包含一个ETag标识符。
            缓存存储：
                    客户端缓存该资源内容，并存储与之关联的ETag值。
            后续请求：
                    当客户端需要再次请求相同资源时，
                    它会在请求头中包含If-None-Match字段，值为之前存储的ETag。
            服务器验证：
                    服务器接收到请求后，
                    比较If-None-Match字段中的ETag与当前资源的ETag。
                    如果两个ETag相同，表示资源未发生变化。
                    服务器返回304 Not Modified状态码，不传输资源内容。
                    如果两个ETag不同，表示资源已更新。
                    服务器返回200 OK状态码，传输最新的资源内容和新的ETag。
            这是一个提高效率的做法！
        */
        NOT_MODIFIED = 304,

        // 400（Bad Request）
        // 由于包含语法错误，当前请求无法被服务器理解。
        // 400通常在服务器端表单验证失败时返回。
        BAD_REQUEST = 400,

        // 404（Not Found）
        // 这太常见了。就是请求所希望得到的资源未被在服务器上发现。
        //当通常用于当服务器不想揭示到底为何请求被拒绝时，
        // 比如应当返回500时服务器不愿透露自己的错误。
        NOT_FOUND = 404,

        // 403（Forbidden）
        // 403状态码表示即使用户已认证，仍然没有权限访问请求的资源。
        // IP地址被封禁 - 请求来自被服务器禁止的IP地址或IP范围。
        // 请求方法被禁用 - 请求使用的HTTP方法（如PUT或DELETE）在服务器配置中被禁用。
        FORBIDDEN = 403,

        // 405（Method Not Allowed）
        // 请求行中指定的请求方法不能被用于请求相应的资源。
        // 在Web开发中通常是因为客户端和服务器的方法不一致，
        // 比如客户端通过PUT来修改一个资源，而服务器把它实现为POST方法。 
        // 开发中统一规范就好了。
        METHOD_NOT_ALLOWED = 405,

        // 客户端accpet的类型，服务器端不支持
        Not_ACCEPTABLE = 406,

        // 500（Internal Server Error）
        // 通常是代码出错，后台Bug。
        // 一般的Web服务器通常会给出抛出异常的调用堆栈。 然而多数服务器即使在生产环境也会打出调用堆栈，这显然是不安全的。
        INTERNAL_SERVER_ERROR = 500,

        // 502（Bad Gateway）
        // 作为网关或者代理工作的服务器尝试执行请求时，从上游服务器接收到无效的响应。
        // 如果你在用HTTP代理来翻墙，或者你配置了nginx来反向代理你的应用，
        // 你可能会常常看到它。
        // （通过代理服务器访问资源时，代理服务器向浏览器返回）
        BAD_GATEWAY = 502,

        // 505 HTTP Version Not Supported
        HTTP_VERSION_NOT_SUPPORTED = 505
    };

        std::unordered_map<HTTPCODE, std::string> http_header_response = {
                {OK, "200 ok"},
                {NOT_MODIFIED, "304 Not Modified"},
                {BAD_REQUEST, "400 Bad Request"},
                {FORBIDDEN, "403 Forbidden"},
                {NOT_FOUND, "404 Not Found"},
                {METHOD_NOT_ALLOWED, "405 Method Not Allowed"},
                {Not_ACCEPTABLE, "406 Not Acceptable"},
                {INTERNAL_SERVER_ERROR, "Internal Server Error"},
                {BAD_GATEWAY, "504 Gateway Timeout"},
                {HTTP_VERSION_NOT_SUPPORTED, "HTTP Version Not Supported"}
        };
} 