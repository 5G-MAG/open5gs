# Copyright (C) 2019,2020 by Sukchan Lee <acetcom@gmail.com>

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
    timer.h

    ogs-mongoc.c
    subscription.c
    session.c
    ims.c
    path.c
    timer.c
'''.split())

# Accepts either mongo-c-driver's pkg-config module name (libmongoc-1.0 for 1.x, mongoc2 for 2.x,
# which also moved its public header from <mongoc.h> to <mongoc/mongoc.h>), and detects which
# header is actually present rather than assuming one. dependency() with more than one name tries
# each in the order given and stops at the first found (Meson FeatureNew 0.60.0, see this branch's
# own meson_version bump).
libmongoc_dep = dependency('libmongoc-1.0', 'mongoc2', required : true)

libdbi_public_c_args = []
if cc.has_header('mongoc/mongoc.h', dependencies : libmongoc_dep)
    libdbi_public_c_args += '-DOGS_HAVE_MONGOC_MONGOC_H'
elif cc.has_header('mongoc.h', dependencies : libmongoc_dep)
    libdbi_public_c_args += '-DOGS_HAVE_MONGOC_H'
else
    error('mongoc header not found')
endif

# Carried on the mongoc dependency, not appended to this library's own c_args, so the library()
# and declare_dependency() calls below keep their existing argument lines unchanged for anything
# that patches against them.
libmongoc_dep = declare_dependency(
    dependencies : libmongoc_dep,
    compile_args : libdbi_public_c_args)

libdbi_inc = include_directories('.')

libdbi = library('ogsdbi',
    sources : libdbi_sources,
    version : libogslib_version,
    c_args : '-DOGS_DBI_COMPILATION',
    include_directories : [libdbi_inc, libinc],
    dependencies : [libcrypt_dep, libapp_dep, libmongoc_dep],
    install : true)

libdbi_dep = declare_dependency(
    link_with : libdbi,
    include_directories : [libdbi_inc, libinc],
    dependencies : [libcrypt_dep, libapp_dep, libmongoc_dep])
