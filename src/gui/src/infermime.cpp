/*  The Clipboard Project - Cut, copy, and paste anything, anywhere, all from the terminal.
    Copyright (C) 2023 Jackson Huff and other contributors on GitHub.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.*/
#include <clipboard/gui.hpp>
#include <optional>
#include <string_view>

using namespace std::string_view_literals;

std::optional<std::string_view> inferMIMEType(const std::string& temporaryContent) {

    std::string_view content(temporaryContent);

    auto beginning_matches = [&](const std::string_view& pattern, const unsigned int offset = 0) {
        if (content.size() < (pattern.size() + offset)) return false;
        return content.substr(0 + offset, pattern.size() + offset) == pattern;
    };

    // jpeg xl
    if (beginning_matches("\x00\x00\x00\x0C\x4A\x58\x4C\x20\x0D\x0A\x87\x0A"sv)) return "image/jxl";

    // xml
    if (beginning_matches("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"sv)) return "text/xml";

    // ico
    if (beginning_matches("\x00\x00\x01\x00"sv)) return "image/x-icon";

    // mpeg 1
    if (beginning_matches("\x00\x00\x01\xB3"sv)) return "video/mpeg";

    // mpeg 2
    if (beginning_matches("\x00\x00\x01\xBA"sv)) return "video/mpeg";

    // utf-32be
    if (beginning_matches("\x00\x00\xFE\xFF"sv)) return "text/plain";

    // ttf
    if (beginning_matches("\x00\x01\x00\x00\x00"sv)) return "font/ttf";

    // xml
    if (beginning_matches("\x00\x3C\x00\x3F\x00\x78\x00\x6D\x00\x6C\x00\x20"sv)) return "text/xml";

    // wasm
    if (beginning_matches("\x00\x61\x73\x6D"sv)) return "application/wasm";

    // jpeg 2000
    if (beginning_matches("\x00\x00\x00\x0C\x6A\x50\x20\x20\x0D\x0A\x87\x0A"sv)) return "image/jp2";

    // jpeg 2000
    if (beginning_matches("\xFF\x4F\xFF\x51"sv)) return "image/jp2";

    // lz4
    if (beginning_matches("\x04\x22\x4D\x18"sv)) return "application/x-lz4";

    // pcap
    if (beginning_matches("\x0A\x0D\x0D\x0A"sv)) return "application/vnd.tcpdump.pcap";

    // winbox
    if (beginning_matches("\x0A\xF0\x1D\xC0"sv)) return "application/x-winbox";

    // lua
    if (beginning_matches("\x1B\x4C\x75\x61"sv)) return "text/x-lua";

    // gzip
    if (beginning_matches("\x1F\x8B"sv)) return "application/gzip";

    // lzw compression
    if (beginning_matches("\x1F\x9D"sv)) return "application/x-lzw";

    // lzh compression
    if (beginning_matches("\x1F\xA0"sv)) return "application/x-lzh";

    // dss
    if (beginning_matches("\x02\x64\x73\x73"sv)) return "audio/dss";

    // deb
    if (beginning_matches("\x21\x3C\x61\x72\x63\x68\x3E\x0A"sv)) return "application/x-deb";

    // postscript
    if (beginning_matches("\x25\x21\x50\x53"sv)) return "application/postscript";

    // pdf
    if (beginning_matches("%PDF-"sv)) return "application/pdf";

    // u-boot
    if (beginning_matches("\x27\x05\x19\x56"sv)) return "application/x-uboot";

    // zstd
    if (beginning_matches("\x28\xB5\x2F\xFD"sv)) return "application/zstd";

    // x.509
    if (beginning_matches("-----BEGIN CERTIFICATE-----"sv) || beginning_matches("-----BEGIN CERTIFICATE REQUEST-----") || beginning_matches("-----BEGIN PRIVATE KEY-----")
        || beginning_matches("-----BEGIN DSA PRIVATE KEY-----") || beginning_matches("-----BEGIN RSA PRIVATE KEY-----"))
        return "application/x-x509-user-cert";

    // lzh file
    if (beginning_matches("-lh0-"sv, 2) || beginning_matches("-lh1"sv, 2) || beginning_matches("-lh2-"sv, 2) || beginning_matches("-lh3-"sv, 2) || beginning_matches("-lh4-"sv, 2)
        || beginning_matches("-lh5-"sv, 2) || beginning_matches("-lhd-"sv, 2))
        return "application/x-lzh";

    // ace
    if (beginning_matches("**ACE**"sv, 7)) return "application/x-ace";

    // utf-7
    if (beginning_matches("+/v8-"sv) || beginning_matches("+/v9-"sv) || beginning_matches("+/v+"sv) || beginning_matches("+/v-"sv)) return "text/plain";

    // html
    if (beginning_matches("<!DOCTYPE html>"sv)) return "text/html";

    // png
    if (beginning_matches("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"sv)) return "image/png";

    // jpeg
    if (beginning_matches("\xFF\xD8\xFF"sv)) return "image/jpeg";

    // gif
    if (beginning_matches("GIF87a"sv) || beginning_matches("GIF89a"sv)) return "image/gif";

    // webp
    if (beginning_matches("RIFF\x00\x00\x00\x00WEBPVP8 "sv)) return "image/webp";

    // bmp
    if (beginning_matches("BM"sv)) return "image/bmp";

    // tiff
    if (beginning_matches("II\x2A\x00"sv) || beginning_matches("MM\x00\x2A"sv)) return "image/tiff";

    // zip
    if (beginning_matches("PK\x03\x04"sv) || beginning_matches("PK\x05\x06"sv) || beginning_matches("PK\x07\x08"sv)) return "application/zip";

    // 7z
    if (beginning_matches("7z\xBC\xAF\x27\x1C"sv)) return "application/x-7z-compressed";

    // rar
    if (beginning_matches("Rar!\x1A\x07\x00"sv)) return "application/vnd.rar";

    // mp3
    if (beginning_matches("ID3"sv)) return "audio/mpeg";

    // mp4
    if (beginning_matches("ftypmp42"sv) || beginning_matches("ftypisom"sv) || beginning_matches("ftypM4V "sv) || beginning_matches("ftypM4A "sv)) return "video/mp4";

    // ogg
    if (beginning_matches("OggS"sv)) return "audio/ogg";

    // flac
    if (beginning_matches("fLaC"sv)) return "audio/flac";

    // tar
    if (beginning_matches("ustar"sv)) return "application/x-tar";

    // xz
    if (beginning_matches("\xFD\x37\x7A\x58\x5A\x00"sv)) return "application/x-xz";

    // bzip2
    if (beginning_matches("BZh"sv)) return "application/x-bzip2";

    // elf
    if (beginning_matches("\x7F\x45\x4C\x46"sv)) return "application/x-executable";

    return std::nullopt;
}
