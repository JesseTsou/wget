/*
 * Copyright(c) 2012 Tim Ruehsen
 *
 * This file is part of MGet.
 *
 * Mget is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mget is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mget.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * IRI/URI routines
 *
 * Changelog
 * 25.04.2012  Tim Ruehsen  created
 *
 */

#include <string.h>
#include <ctype.h>

#include "xalloc.h"
#include "utils.h"
#include "log.h"
#include "iri.h"

#define IRI_CTYPE_GENDELIM (1<<0)
#define _iri_isgendelim(c) (iri_ctype[(unsigned char)(c)]&IRI_CTYPE_GENDELIM)

#define IRI_CTYPE_SUBDELIM (1<<0)
#define _iri_issubdelim(c) (iri_ctype[(unsigned char)(c)]&IRI_CTYPE_SUBDELIM)

static const unsigned char
	iri_ctype[256] = {
		[':'] = IRI_CTYPE_GENDELIM,
		['/'] = IRI_CTYPE_GENDELIM,
		['?'] = IRI_CTYPE_GENDELIM,
		['#'] = IRI_CTYPE_GENDELIM,
		['['] = IRI_CTYPE_GENDELIM,
		[']'] = IRI_CTYPE_GENDELIM,
		['@'] = IRI_CTYPE_GENDELIM,

		['!'] = IRI_CTYPE_SUBDELIM,
		['$'] = IRI_CTYPE_SUBDELIM,
		['&'] = IRI_CTYPE_SUBDELIM,
		['\\'] = IRI_CTYPE_SUBDELIM,
		['\''] = IRI_CTYPE_SUBDELIM,
		['('] = IRI_CTYPE_SUBDELIM,
		[')'] = IRI_CTYPE_SUBDELIM,
		['*'] = IRI_CTYPE_SUBDELIM,
		['+'] = IRI_CTYPE_SUBDELIM,
		[','] = IRI_CTYPE_SUBDELIM,
		[';'] = IRI_CTYPE_SUBDELIM,
		['='] = IRI_CTYPE_SUBDELIM
	};

int iri_isgendelim(char c)
{
	// return strchr(":/?#[]@",c)!=NULL;
	return _iri_isgendelim(c);
}

int iri_issubdelim(char c)
{
	// return strchr("!$&\'()*+,;=",c)!=NULL;
	return _iri_issubdelim(c);
}

int iri_isreserved(char c)
{
	return iri_isgendelim(c) || iri_issubdelim(c);
}

int iri_isunreserved(char c)
{
	return isalnum(c) || strchr("-._~", c) != NULL;
}

void iri_free(IRI **iri)
{
	if (iri && *iri) {
		xfree(*iri);
	}
}

IRI *iri_parse(const char *s_uri)
{
	IRI *iri;
	char *p, *s, *authority, c;
	int slen;

	/*
		URI         = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
		hier-part   = "//" authority path-abempty / path-absolute / path-rootless / path-empty
		scheme      =  ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
	 */
	while (isspace(*s_uri)) s_uri++;

	// just use one block of memory for all parsed URI parts
	slen = strlen(s_uri);
	iri = xmalloc(sizeof(IRI) + slen * 2 + 2);
	memset(iri, 0, sizeof(IRI));
	strcpy(((char *)iri) + sizeof(IRI), s_uri);
	strcpy(((char *)iri) + sizeof(IRI) + slen + 1, s_uri);
	iri->uri = ((char *)iri) + sizeof(IRI) + slen + 1;
	s = ((char *)iri) + sizeof(IRI);

	p = s;
	while (*s && !_iri_isgendelim(*s))
		s++;

	if (*s == ':') {
		// found a scheme
		iri->scheme = p;
		*s++ = 0;
	} else
		s = p; // rewind

	// this is true for http, https, ftp, file
	if (s[0] == '/' && s[1] == '/')
		s += 2;

	// authority
	authority = s;
	while (*s && *s != '/' && *s != '?' && *s != '#')
		s++;
	c = *s;
	if (c) *s++ = 0;

	// left over: [path][?query][#fragment]
	if (c == '/') {
		iri->path = s;
		while (*s && *s != '?' && *s != '#')
			s++;
		c = *s;
		if (c) *s++ = 0;
	}

	if (c == '?') {
		iri->query = s;
		while (*s && *s != '#')
			s++;
		c = *s;
		if (c) *s++ = 0;
	}

	if (c == '#') {
		iri->fragment = s;
		while (*s)
			s++;
	}

	if (*s) {
		log_printf("unparsed rest '%s'\n", s);
	}

	if (*authority) {
		s = authority;
		p = strchr(authority, '@');
		if (p) {
			iri->userinfo = s;
			*p = 0;
			s = p + 1;
		}
		if (*s == '[') {
			p = strrchr(s, ']');
			if (p) {
				iri->host = s;
				s = p + 1;
			} else {
				// something is broken
				iri->host = s;
				while (*s) s++;
			}
		} else {
			iri->host = s;
			while (*s && *s != ':')
				s++;
		}
		if (*s == ':') {
			if (s[1])
				iri->port = s + 1;
		}
		*s = 0;
	}

	return iri;
}

char *iri_get_connection_part(IRI *iri, char *tag, size_t tagsize)
{
	size_t len;

	if (iri->scheme) {
		if (iri->port) {
			len = strlen(iri->scheme) + strlen(iri->host) + strlen(iri->port) + 4 + 1;
			if (len > tagsize)
				tag = xmalloc(len);

			sprintf(tag, "%s://%s:%s", iri->scheme, iri->host, iri->port);
		} else {
			len = strlen(iri->scheme) + strlen(iri->host) + 3 + 1;
			if (len > tagsize)
				tag = xmalloc(len);

			sprintf(tag, "%s://%s", iri->scheme, iri->host);
		}
	} else {
		if (iri->port) {
			len = strlen(iri->host) + strlen(iri->port) + 1 + 1;
			if (len > tagsize)
				tag = xmalloc(len);

			sprintf(tag, "%s:%s", iri->host, iri->port);
		} else {
			len = strlen(iri->host) + 1;
			if (len > tagsize)
				tag = xmalloc(len);

			sprintf(tag, "%s", iri->host);
		}
	}

	if (len > tagsize)
		return tag; // return allocated buffer

	return NULL; // static buffer was large enough
}

// normalize /../ and remove /./

static size_t _normalize_path(char *path)
{
	char *p1 = path, *p2 = path;

	log_printf("path %s ->\n", path);

	// skip ./ and ../ at the beginning of the path
	while (*p2 == '.' || *p2 == '/') {
		if (*p2 == '/')
			p2++;
		else if (*p2 == '.') {
			if (p2[1] == '/')
				p2 += 2;
			else if (p2[1] == '.' && p2[2] == '/')
				p2 += 3;
			else
				break;
		} else
			break;
	}

	while (*p2) {
		if (*p2 == '/') {
			if (p2[1] == '.') {
				if (!strncmp(p2, "/../", 4)) {
					// go one level up
					p2 += 3;
					while (p1 > path && *--p1 != '/');
				} else if (!strcmp(p2, "/..")) {
					p2 += 3;
					while (p1 > path && *--p1 != '/');
				} else if (!strncmp(p2, "/./", 3)) {
					p2 += 2;
				} else if (!strcmp(p2, "/.")) {
					p2 += 2;
				} else
					*p1++ = *p2++;
			} else if (p1 == path)
				p2++; // avoid leading slash
			else if (p2[1] == '/')
				p2++; // double slash to single slash
			else
				*p1++ = *p2++;
		} else
			*p1++ = *p2++;
	}
	*p1 = 0;
	log_printf("     %s\n", path);

	return p1 - path;
}

// convert relative URI to absolute URI

char *iri_relative_to_absolute(IRI *iri, const char *tag, const char *val, size_t len, char *dst, size_t dst_size)
{
	size_t dst_len = 0;

	log_printf("*url = %.*s\n", (int)len, val);

	if (*val == '/') {
		char path[len + 1];

		strlcpy(path, val, len + 1);

		if (len >= 2 && val[1] == '/') {
			char *p;

			// absolute URI without scheme: //authority/path...
			if ((p = strchr(path + 2, '/')))
				dst_len = _normalize_path(p + 1) + (p - path) + 1;

			dst_len += strlen(iri->scheme ? iri->scheme : "http") + 1 + 1;
			if (dst_len > dst_size)
				dst = xmalloc(dst_len);

			snprintf(dst, dst_len, "%s:%s", iri->scheme ? iri->scheme : "http", path);
			log_printf("*1 %s\n", dst);
		} else {
			// absolute path
			dst_len = strlen(tag) + _normalize_path(path) + 1 + 1;
			if (dst_len > dst_size)
				dst = xmalloc(dst_len);

			snprintf(dst, dst_len, "%s/%s", tag, path);
			log_printf("*2 %s\n", dst);
		}
	} else {
		// see if URI begins with a scheme:
		if (memchr(val, ':', len)) {
			// absolute URI
			dst_len = len + 1;
			if (dst_len > dst_size)
				dst = xmalloc(dst_len);

			strlcpy(dst, val, dst_len);
			log_printf("*3 %s\n", dst);
		} else {
			// relative URI
			const char *lastsep = iri->path ? strrchr(iri->path, '/') : NULL;
			int pathlen = lastsep ? (lastsep - iri->path) + len + 2 : len + 1;
			char path[pathlen];

			if (lastsep) {
				snprintf(path, pathlen, "%.*s%.*s", (int)(lastsep - iri->path + 1), iri->path, (int)len, val);
			} else {
				strlcpy(path, val, len + 1);
			}

			dst_len = strlen(tag) + _normalize_path(path) + 1 + 1;
			if (dst_len > dst_size)
				dst = xmalloc(dst_len);

			snprintf(dst, dst_len, "%s/%s", tag, path);
			log_printf("*4 %s\n", dst);
		}
	}

	if (dst_len > dst_size)
		return dst;

	return NULL;
}
