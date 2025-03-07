// MinIO C++ Library for Amazon S3 Compatible Cloud Storage
// Copyright 2022 MinIO, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "utils.h"

const char* HTTP_HEADER_FORMAT = "%a, %d %b %Y %H:%M:%S GMT";
const std::regex MULTI_SPACE_REGEX("( +)");
const std::regex VALID_BUCKET_NAME_REGEX(
    "^[A-Za-z0-9][A-Za-z0-9_\\.\\-\\:]{1,61}[A-Za-z0-9]$");
const std::regex VALID_BUCKET_NAME_STRICT_REGEX(
    "^[a-z0-9][a-z0-9\\.\\-]{1,61}[a-z0-9]$");
const std::regex VALID_IP_ADDR_REGEX("^(\\d+\\.){3}\\d+$");

bool minio::utils::GetEnv(std::string& var, const char* name) {
  if (const char* value = std::getenv(name)) {
    var = value;
    return true;
  }
  return false;
}

std::string minio::utils::GetHomeDir() {
  std::string home;
  if (GetEnv(home, "HOME")) return home;
  return getpwuid(getuid())->pw_dir;
}

std::string minio::utils::Printable(std::string s) {
  std::stringstream ss;
  for (auto& ch : s) {
    if (ch < 33 || ch > 126) {
      ss << "\\x" << std::hex << std::setfill('0') << std::setw(2)
         << (ch & 0xff);
    } else {
      ss << ch;
    }
  }

  return ss.str();
}

unsigned long minio::utils::CRC32(std::string_view str) {
  return crc32(0, (const unsigned char*)str.data(), str.size());
}

unsigned int minio::utils::Int(std::string_view str) {
  unsigned char* data = (unsigned char*)str.data();
  return data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
}

bool minio::utils::StringToBool(std::string str) {
  std::string s = ToLower(str);
  if (s == "false") return false;
  if (s == "true") return true;

  std::cerr << "ABORT: Unknown bool string. This should not happen."
            << std::endl;
  std::terminate();

  return false;
}

std::string minio::utils::Trim(std::string_view str, char ch) {
  int start, len;
  for (start = 0; start < str.size() && str[start] == ch; start++)
    ;
  for (len = str.size() - start; len > 0 && str[start + len - 1] == ch; len--)
    ;
  return std::string(str.substr(start, len));
}

bool minio::utils::CheckNonEmptyString(std::string_view str) {
  return !str.empty() && Trim(str) == str;
}

std::string minio::utils::ToLower(std::string str) {
  std::string s = str;
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  return s;
}

bool minio::utils::StartsWith(std::string_view str, std::string_view prefix) {
  return (str.size() >= prefix.size() &&
          str.compare(0, prefix.size(), prefix) == 0);
}

bool minio::utils::EndsWith(std::string_view str, std::string_view suffix) {
  return (str.size() >= suffix.size() &&
          str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

bool minio::utils::Contains(std::string_view str, char ch) {
  return str.find(ch) != std::string::npos;
}

bool minio::utils::Contains(std::string_view str, std::string_view substr) {
  return str.find(substr) != std::string::npos;
}

std::string minio::utils::Join(std::list<std::string> values,
                               std::string delimiter) {
  std::string result;
  for (const auto& value : values) {
    if (!result.empty()) result += delimiter;
    result += value;
  }
  return result;
}

std::string minio::utils::Join(std::vector<std::string> values,
                               std::string delimiter) {
  std::string result;
  for (const auto& value : values) {
    if (!result.empty()) result += delimiter;
    result += value;
  }
  return result;
}

std::string minio::utils::EncodePath(std::string& path) {
  std::stringstream str_stream(path);
  std::string token;
  std::string out;
  while (std::getline(str_stream, token, '/')) {
    if (!token.empty()) {
      if (!out.empty()) out += "/";
      out += curlpp::escape(token);
    }
  }

  if (*(path.begin()) == '/') out = "/" + out;
  if (*(path.end() - 1) == '/' && out != "/") out += "/";

  return out;
}

std::string minio::utils::Sha256Hash(std::string_view str) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_create();
  if (ctx == NULL) {
    std::cerr << "failed to create EVP_MD_CTX" << std::endl;
    std::terminate();
  }

  if (1 != EVP_DigestInit_ex(ctx, EVP_sha256(), NULL)) {
    std::cerr << "failed to init SHA-256 digest" << std::endl;
    std::terminate();
  }

  if (1 != EVP_DigestUpdate(ctx, str.data(), str.size())) {
    std::cerr << "failed to update digest" << std::endl;
    std::terminate();
  }

  unsigned int length = EVP_MD_size(EVP_sha256());
  unsigned char* digest = (unsigned char*)OPENSSL_malloc(length);
  if (digest == NULL) {
    std::cerr << "failed to allocate memory for hash value" << std::endl;
    std::terminate();
  }

  if (1 != EVP_DigestFinal_ex(ctx, digest, &length)) {
    OPENSSL_free(digest);
    std::cerr << "failed to finalize digest" << std::endl;
    std::terminate();
  }

  EVP_MD_CTX_destroy(ctx);

  std::string hash;
  char buf[3];
  for (int i = 0; i < length; ++i) {
    sprintf(buf, "%02x", digest[i]);
    hash += buf;
  }

  OPENSSL_free(digest);

  return hash;
}

std::string minio::utils::Base64Encode(std::string_view str) {
  const auto base64_memory = BIO_new(BIO_s_mem());
  auto base64 = BIO_new(BIO_f_base64());
  base64 = BIO_push(base64, base64_memory);

  BIO_write(base64, str.data(), str.size());
  BIO_flush(base64);

  BUF_MEM* buf_mem{};
  BIO_get_mem_ptr(base64, &buf_mem);
  auto base64_encoded = std::string(buf_mem->data, buf_mem->length - 1);

  BIO_free_all(base64);

  return base64_encoded;
}

std::string minio::utils::Md5sumHash(std::string_view str) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_create();
  if (ctx == NULL) {
    std::cerr << "failed to create EVP_MD_CTX" << std::endl;
    std::terminate();
  }

  if (1 != EVP_DigestInit_ex(ctx, EVP_md5(), NULL)) {
    std::cerr << "failed to init MD5 digest" << std::endl;
    std::terminate();
  }

  if (1 != EVP_DigestUpdate(ctx, str.data(), str.size())) {
    std::cerr << "failed to update digest" << std::endl;
    std::terminate();
  }

  unsigned int length = EVP_MD_size(EVP_md5());
  unsigned char* digest = (unsigned char*)OPENSSL_malloc(length);
  if (digest == NULL) {
    std::cerr << "failed to allocate memory for hash value" << std::endl;
    std::terminate();
  }

  if (1 != EVP_DigestFinal_ex(ctx, digest, &length)) {
    OPENSSL_free(digest);
    std::cerr << "failed to finalize digest" << std::endl;
    std::terminate();
  }

  EVP_MD_CTX_destroy(ctx);

  std::string hash = std::string((const char*)digest, length);
  OPENSSL_free(digest);

  return minio::utils::Base64Encode(hash);
}

std::string minio::utils::FormatTime(const std::tm* time, const char* format) {
  char buf[128];
  std::strftime(buf, 128, format, time);
  return std::string(buf);
}

std::tm* minio::utils::Time::ToUTC() {
  std::tm* t = new std::tm;
  *t = utc_ ? *std::localtime(&tv_.tv_sec) : *std::gmtime(&tv_.tv_sec);
  return t;
}

std::string minio::utils::Time::ToSignerDate() {
  std::tm* utc = ToUTC();
  std::string result = FormatTime(utc, "%Y%m%d");
  delete utc;
  return result;
}

std::string minio::utils::Time::ToAmzDate() {
  std::tm* utc = ToUTC();
  std::string result = FormatTime(utc, "%Y%m%dT%H%M%SZ");
  delete utc;
  return result;
}

std::string minio::utils::Time::ToHttpHeaderValue() {
  std::tm* utc = ToUTC();
  std::locale("C");
  std::string result = FormatTime(utc, HTTP_HEADER_FORMAT);
  std::locale("");
  delete utc;
  return result;
}

minio::utils::Time minio::utils::Time::FromHttpHeaderValue(const char* value) {
  std::tm t{0};
  std::locale("C");
  strptime(value, HTTP_HEADER_FORMAT, &t);
  std::locale("");
  std::time_t time = std::mktime(&t);
  return Time(time, 0, true);
}

std::string minio::utils::Time::ToISO8601UTC() {
  char buf[64];
  snprintf(buf, 64, "%03ld", tv_.tv_usec);
  std::string usec_str(buf);
  if (usec_str.size() > 3) usec_str = usec_str.substr(0, 3);
  std::tm* utc = ToUTC();
  std::string result = FormatTime(utc, "%Y-%m-%dT%H:%M:%S.") + usec_str + "Z";
  delete utc;
  return result;
}

minio::utils::Time minio::utils::Time::FromISO8601UTC(const char* value) {
  std::tm t{0};
  suseconds_t tv_usec = 0;
  char* rv = strptime(value, "%Y-%m-%dT%H:%M:%S", &t);
  sscanf(rv, ".%lu", &tv_usec);
  std::time_t time = std::mktime(&t);
  return Time(time, tv_usec, true);
}

void minio::utils::Multimap::Add(std::string key, std::string value) {
  map_[key].insert(value);
  keys_[ToLower(key)].insert(key);
}

void minio::utils::Multimap::AddAll(const Multimap& headers) {
  auto m = headers.map_;
  for (auto& [key, values] : m) {
    map_[key].insert(values.begin(), values.end());
    keys_[ToLower(key)].insert(key);
  }
}

std::list<std::string> minio::utils::Multimap::ToHttpHeaders() {
  std::list<std::string> headers;
  for (auto& [key, values] : map_) {
    for (auto& value : values) {
      headers.push_back(key + ": " + value);
    }
  }
  return headers;
}

std::string minio::utils::Multimap::ToQueryString() {
  std::string query_string;
  for (auto& [key, values] : map_) {
    for (auto& value : values) {
      std::string s = curlpp::escape(key) + "=" + curlpp::escape(value);
      if (!query_string.empty()) query_string += "&";
      query_string += s;
    }
  }
  return query_string;
}

bool minio::utils::Multimap::Contains(std::string_view key) {
  return keys_.find(ToLower(std::string(key))) != keys_.end();
}

std::list<std::string> minio::utils::Multimap::Get(std::string_view key) {
  std::list<std::string> result;
  std::set<std::string> keys = keys_[ToLower(std::string(key))];
  for (auto& key : keys) {
    std::set<std::string> values = map_[key];
    result.insert(result.end(), values.begin(), values.end());
  }
  return result;
}

std::string minio::utils::Multimap::GetFront(std::string_view key) {
  std::list<std::string> values = Get(key);
  return (values.size() > 0) ? values.front() : "";
}

std::list<std::string> minio::utils::Multimap::Keys() {
  std::list<std::string> keys;
  for (const auto& [key, _] : keys_) keys.push_back(key);
  return keys;
}

void minio::utils::Multimap::GetCanonicalHeaders(
    std::string& signed_headers, std::string& canonical_headers) {
  std::vector<std::string> signed_headerslist;
  std::map<std::string, std::string> map;

  for (auto& [k, values] : map_) {
    std::string key = ToLower(k);
    if ("authorization" == key || "user-agent" == key) continue;
    if (std::find(signed_headerslist.begin(), signed_headerslist.end(), key) ==
        signed_headerslist.end()) {
      signed_headerslist.push_back(key);
    }

    std::string value;
    for (auto& v : values) {
      if (!value.empty()) value += ",";
      value += std::regex_replace(v, MULTI_SPACE_REGEX, " ");
    }

    map[key] = value;
  }

  std::sort(signed_headerslist.begin(), signed_headerslist.end());
  signed_headers = utils::Join(signed_headerslist, ";");

  std::vector<std::string> canonical_headerslist;
  for (auto& [key, value] : map) {
    canonical_headerslist.push_back(key + ":" + value);
  }

  std::sort(canonical_headerslist.begin(), canonical_headerslist.end());
  canonical_headers = utils::Join(canonical_headerslist, "\n");
}

std::string minio::utils::Multimap::GetCanonicalQueryString() {
  std::vector<std::string> keys;
  for (auto& [key, _] : map_) keys.push_back(key);
  std::sort(keys.begin(), keys.end());

  std::vector<std::string> values;
  for (auto& key : keys) {
    auto vals = map_[key];
    for (auto& value : vals) {
      std::string s = curlpp::escape(key) + "=" + curlpp::escape(value);
      values.push_back(s);
    }
  }

  return utils::Join(values, "&");
}

minio::error::Error minio::utils::CheckBucketName(std::string_view bucket_name,
                                                  bool strict) {
  if (Trim(bucket_name).empty()) {
    return error::Error("bucket name cannot be empty");
  }

  if (bucket_name.length() < 3) {
    return error::Error("bucket name cannot be less than 3 characters");
  }

  if (bucket_name.length() > 63) {
    return error::Error("Bucket name cannot be greater than 63 characters");
  }

  if (std::regex_match(bucket_name.data(), VALID_IP_ADDR_REGEX)) {
    return error::Error("bucket name cannot be an IP address");
  }

  // unallowed successive characters check.
  if (Contains(bucket_name, "..") || Contains(bucket_name, ".-") ||
      Contains(bucket_name, "-.")) {
    return error::Error(
        "Bucket name contains invalid successive characters '..', '.-' or "
        "'-.'");
  }

  if (strict) {
    if (!std::regex_match(bucket_name.data(), VALID_BUCKET_NAME_STRICT_REGEX)) {
      return error::Error("bucket name does not follow S3 standards strictly");
    }
  } else if (!std::regex_match(bucket_name.data(), VALID_BUCKET_NAME_REGEX)) {
    return error::Error("bucket name does not follow S3 standards");
  }

  return error::SUCCESS;
}

minio::error::Error minio::utils::ReadPart(std::istream& stream, char* buf,
                                           size_t size, size_t& bytes_read) {
  stream.read(buf, size);
  bytes_read = stream.gcount();
  return minio::error::SUCCESS;
}

minio::error::Error minio::utils::CalcPartInfo(long object_size,
                                               size_t& part_size,
                                               long& part_count) {
  if (part_size > 0) {
    if (part_size < kMinPartSize) {
      return error::Error("part size " + std::to_string(part_size) +
                          " is not supported; minimum allowed 5MiB");
    }

    if (part_size > kMaxPartSize) {
      return error::Error("part size " + std::to_string(part_size) +
                          " is not supported; maximum allowed 5GiB");
    }
  }

  if (object_size >= 0) {
    if (object_size > kMaxObjectSize) {
      return error::Error("object size " + std::to_string(object_size) +
                          " is not supported; maximum allowed 5TiB");
    }
  } else if (part_size <= 0) {
    return error::Error(
        "valid part size must be provided when object size is unknown");
  }

  if (object_size < 0) {
    part_count = -1;
    return error::SUCCESS;
  }

  if (part_size <= 0) {
    // Calculate part size by multiple of kMinPartSize.
    double psize = std::ceil((double)object_size / kMaxMultipartCount);
    part_size = (size_t)std::ceil(psize / kMinPartSize) * kMinPartSize;
  }

  if (part_size > object_size) part_size = object_size;
  part_count =
      part_size > 0 ? (long)std::ceil((double)object_size / part_size) : 1;
  if (part_count > kMaxMultipartCount) {
    return error::Error(
        "object size " + std::to_string(object_size) + " and part size " +
        std::to_string(part_size) + " make more than " +
        std::to_string(kMaxMultipartCount) + "parts for upload");
  }

  return error::SUCCESS;
}
