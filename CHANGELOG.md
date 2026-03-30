# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-03-30

### Added
- Initial release of nginx-gone module
- `gone on|off` directive for per-context enable/disable (http, server, location)
- `gone_map_file` directive for loading URI list from file (http context)
- `gone_use_request_uri on|off` directive for matching `$request_uri` instead of `$uri`
- Exact URI matching via nginx hash table (O(1) lookup)
- Regex URI matching via PCRE (`~` case-sensitive, `~*` case-insensitive)
- Server rewrite phase handler for early request interception
- Config-time file parsing (read once at startup/reload)
- nginx `map` file format compatibility (second column ignored)
- Support for comments (`#`) and blank lines in map files
- Dynamic module support (`--add-dynamic-module`)
- Debian packaging
- OBS (openSUSE Build Service) packaging
- Test suite (basic, regex, negative tests)
