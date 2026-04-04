[![OBS Build Status](https://build.opensuse.org/projects/home:rumenx/packages/nginx-gone/badge.svg?type=default)](https://build.opensuse.org/package/show/home:rumenx/nginx-gone)
[![GitHub Release](https://img.shields.io/github/v/release/RumenDamyanov/nginx-gone?label=Release)](https://github.com/RumenDamyanov/nginx-gone/releases)
[![License](https://img.shields.io/github/license/RumenDamyanov/nginx-gone?label=License)](LICENSE.md)
[![Platform Support](https://img.shields.io/badge/Platform-Debian%20%7C%20Ubuntu%20%7C%20Fedora%20%7C%20openSUSE%20%7C%20RHEL-blue)](https://build.opensuse.org/package/show/home:rumenx/nginx-gone)

# Nginx Gone

A lightweight Nginx module that returns **HTTP 410 Gone** for URIs listed in a map file. Cleaner and faster than `map` + `if` for managing permanently removed URLs.

## Features

- **Simple map file** — one URI per line, read once at config load
- **Exact matching** — O(1) hash table lookup for exact URIs
- **Regex matching** — PCRE patterns with `~` (case-sensitive) and `~*` (case-insensitive)
- **Per-context control** — `gone on|off` at http, server, or location level
- **Early interception** — server rewrite phase, before location matching
- **No dependencies** — pure nginx API, no external libraries
- **Dynamic module** — works with `load_module`
- **nginx map compatible** — reuse existing map files (second column ignored)

## Documentation & Support

📖 **[Complete Documentation](https://github.com/RumenDamyanov/nginx-gone/wiki)** - Guides, tutorials, and reference

💬 **[Community Discussions](https://github.com/RumenDamyanov/nginx-gone/discussions)** - Questions, help, and feedback

### Quick Links

- 🏠 **[Home](https://github.com/RumenDamyanov/nginx-gone/wiki/Home)** - Overview and getting started
- 📦 **[Installation Guide](https://github.com/RumenDamyanov/nginx-gone/wiki/Installation-Guide)** - Step-by-step installation
- 🔨 **[Building from Source](https://github.com/RumenDamyanov/nginx-gone/wiki/Building-from-Source)** - Compile the module
- 📋 **[Configuration Reference](https://github.com/RumenDamyanov/nginx-gone/wiki/Configuration-Reference)** - All directives
- ⚙️ **[Basic Configuration](https://github.com/RumenDamyanov/nginx-gone/wiki/Basic-Configuration)** - Simple setup examples
- 🔧 **[Troubleshooting](https://github.com/RumenDamyanov/nginx-gone/wiki/Troubleshooting)** - Common issues and solutions

## Repository Structure

- `src/` — Nginx module source code
- `debian/` — Packaging files (for building .deb packages)
- `conf/` — Example configuration

## Quick Start

### 1. Install

Pre-built packages are available via the [openSUSE Build Service](https://build.opensuse.org/package/show/home:rumenx/nginx-gone).

**Debian/Ubuntu:**
```bash
# Add the OBS repository (see Installation Guide for details)
apt install nginx-gone
```

**Fedora/RHEL:**
```bash
# Add the OBS repository (see Installation Guide for details)
dnf install nginx-gone
```

### 2. Create a map file

```bash
cat > /etc/nginx/maps/gone.map <<EOF
# Removed blog posts
/blog/2020/deleted-post 1
/blog/2019/old-article 1

# Discontinued products
/products/old-widget 1

# Regex: everything under /deprecated/
~^/deprecated/ 1

# Case-insensitive: old PHP pages
~*^/old-(.*)\.php$ 1
EOF
```

### 3. Configure nginx

```nginx
load_module modules/ngx_http_gone_module.so;

http {
    gone_map_file /etc/nginx/maps/gone.map;

    server {
        listen 80;
        server_name example.com;
        gone on;

        location / {
            root /var/www/html;
            try_files $uri $uri/ =404;
        }
    }
}
```

### 4. Test

```bash
nginx -t && nginx -s reload
curl -I http://example.com/blog/2020/deleted-post
# HTTP/1.1 410 Gone
```

## Configuration Reference

| Directive | Context | Default | Description |
|-----------|---------|---------|-------------|
| `gone` | http, server, location | `off` | Enable/disable gone checking |
| `gone_map_file` | http | — | Path to URI list file |
| `gone_use_request_uri` | http | `off` | Match `$request_uri` instead of `$uri` |

## Map File Format

```
# Comments start with #
/exact/path 1              # Exact match
~^/regex/pattern 1         # Case-sensitive regex
~*^/case-insensitive 1     # Case-insensitive regex
```

- One URI per line
- Second column is optional (ignored, for nginx `map` compatibility)
- `~` prefix = case-sensitive PCRE regex
- `~*` prefix = case-insensitive PCRE regex
- Everything else = exact string match

## Building from Source

```bash
# Get nginx source
wget https://nginx.org/download/nginx-1.27.0.tar.gz
tar xzf nginx-1.27.0.tar.gz
cd nginx-1.27.0

# Build as dynamic module
./configure --add-dynamic-module=/path/to/nginx-gone/src
make modules

# Install
cp objs/ngx_http_gone_module.so /usr/lib/nginx/modules/
```

## Related Projects

Other nginx dynamic modules we maintain:

| Module | Description | GitHub | OBS |
|--------|-------------|--------|-----|
| **nginx-torblocker** | Control access from Tor exit nodes — block, allow, or Tor-only mode | [GitHub](https://github.com/RumenDamyanov/nginx-torblocker) | [OBS](https://build.opensuse.org/package/show/home:rumenx/nginx-torblocker) |
| **nginx-cf-realip** | Automatic Cloudflare edge IP list fetcher for real client IP restoration | [GitHub](https://github.com/RumenDamyanov/nginx-cf-realip) | [OBS](https://build.opensuse.org/package/show/home:rumenx/nginx-cf-realip) |
| **nginx-waf** | IP/CIDR-based access control with named lists and tag-based organization | [GitHub](https://github.com/RumenDamyanov/nginx-waf) | [OBS](https://build.opensuse.org/package/show/home:rumenx/nginx-waf) |
| **nginx-waf-api** | REST API daemon for dynamic nginx-waf IP list management | [GitHub](https://github.com/RumenDamyanov/nginx-waf-api) | [OBS](https://build.opensuse.org/package/show/home:rumenx/nginx-waf-api) |
| **nginx-waf-feeds** | Automatic threat feed updater for nginx-waf | [GitHub](https://github.com/RumenDamyanov/nginx-waf-feeds) | [OBS](https://build.opensuse.org/package/show/home:rumenx/nginx-waf-feeds) |
| **nginx-waf-ui** | Web management interface for nginx-waf | [GitHub](https://github.com/RumenDamyanov/nginx-waf-ui) | [OBS](https://build.opensuse.org/package/show/home:rumenx/nginx-waf-ui) |
| **nginx-waf-lua** | OpenResty/Lua integration for nginx-waf | [GitHub](https://github.com/RumenDamyanov/nginx-waf-lua) | [OBS](https://build.opensuse.org/package/show/home:rumenx/nginx-waf-lua) |

### Apache HTTP Server Versions

All modules are also available for Apache httpd:

| Module | Description | GitHub | OBS |
|--------|-------------|--------|-----|
| **apache-gone** | Return HTTP 410 Gone for permanently removed URIs | [GitHub](https://github.com/RumenDamyanov/apache-gone) | [OBS](https://build.opensuse.org/package/show/home:rumenx/apache-gone) |
| **apache-cf-realip** | Cloudflare real IP restoration via `mod_remoteip` | [GitHub](https://github.com/RumenDamyanov/apache-cf-realip) | [OBS](https://build.opensuse.org/package/show/home:rumenx/apache-cf-remoteip) |
| **apache-torblocker** | Control access from Tor exit nodes | [GitHub](https://github.com/RumenDamyanov/apache-torblocker) | [OBS](https://build.opensuse.org/package/show/home:rumenx/apache-torblocker) |
| **apache-waf** | IP/CIDR-based access control with named lists | [GitHub](https://github.com/RumenDamyanov/apache-waf) | [OBS](https://build.opensuse.org/package/show/home:rumenx/apache-waf) |
| **apache-waf-api** | REST API for dynamic WAF IP list management | [GitHub](https://github.com/RumenDamyanov/apache-waf-api) | [OBS](https://build.opensuse.org/package/show/home:rumenx/apache-waf-api) |
| **apache-waf-feeds** | Automatic threat feed updater for apache-waf | [GitHub](https://github.com/RumenDamyanov/apache-waf-feeds) | [OBS](https://build.opensuse.org/package/show/home:rumenx/apache-waf-feeds) |
| **apache-waf-ui** | Web management interface for apache-waf | [GitHub](https://github.com/RumenDamyanov/apache-waf-ui) | [OBS](https://build.opensuse.org/package/show/home:rumenx/apache-waf-ui) |

## License

BSD 3-Clause License. See [LICENSE.md](LICENSE.md).
