# Copyright (C) 2019-2023 by Sukchan Lee <acetcom@gmail.com>

# This file is part of Open5GS.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

libdbi_sources = files('''
    ogs-dbi.h

    ogs-mongoc.h

    ogs-mongoc.c
    subscription.c
    session.c
    ims.c
'''.split())

# mongo-c-driver 2.x renamed its pkg-config module from libmongoc-1.0 to mongoc2 and moved the
# public header from <mongoc.h> to <mongoc/mongoc.h>. Distributions have begun shipping only 2.x:
# Ubuntu 26.04's libmongoc-dev is 2.2.2 and installs mongoc2.pc alone, so requiring libmongoc-1.0
# outright fails at configure time there with
#   ERROR: Dependency "libmongoc-1.0" not found (tried pkg-config and cmake)
# Both modules are accepted, and the header actually present is detected rather than assumed. This
# matches what upstream open5gs does in the same file.
libmongoc_dep = dependency('libmongoc-1.0', required : false)
if not libmongoc_dep.found()
    libmongoc_dep = dependency('mongoc2')
endif

libdbi_public_c_args = []
if cc.has_header('mongoc/mongoc.h', dependencies : libmongoc_dep)
    libdbi_public_c_args += '-DOGS_HAVE_MONGOC_MONGOC_H'
elif cc.has_header('mongoc.h', dependencies : libmongoc_dep)
    libdbi_public_c_args += '-DOGS_HAVE_MONGOC_H'
else
    error('mongoc header not found')
endif

# The macro rides on the mongoc dependency rather than being appended to this library's c_args.
# ogs-mongoc.h is public, so every consumer needs it, and carrying it here means the library() and
# declare_dependency() calls below keep the exact argument lines that consumers of this tree patch
# against: rt-mbs-function's open5gs.patch adds a soversion line using 'c_args :
# '-DOGS_DBI_COMPILATION',' as context, and editing that line makes the patch fail to apply.
libmongoc_dep = declare_dependency(
    dependencies : libmongoc_dep,
    compile_args : libdbi_public_c_args)

libdbi_inc = include_directories('.')

libdbi = library('ogsdbi',
    sources : libdbi_sources,
    version : libogslib_version,
    c_args : '-DOGS_DBI_COMPILATION',
    include_directories : [libdbi_inc, libinc],
    dependencies : [libproto_dep, libmongoc_dep],
    install : true)

libdbi_dep = declare_dependency(
    link_with : libdbi,
    include_directories : [libdbi_inc, libinc],
    dependencies : [libproto_dep, libmongoc_dep])
