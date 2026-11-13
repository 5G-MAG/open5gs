libmulticastrouter_sources = files('''
  multicastrouter.c
  multicastrouter.h
'''.split())

libmulticastrouter_inc = include_directories('.')

libmulticastrouter = library('ogsmulticastrouter',
    sources : libmulticastrouter_sources,
    version : libogslib_version,
    c_args : '-DOGS_MULTICASTROUTER_COMPILATION',
    include_directories : [libmulticastrouter_inc, libinc],
    dependencies : libcore_dep,
    install : true)

libmulticastrouter_dep = declare_dependency(
    link_with : libmulticastrouter,
    include_directories : [libmulticastrouter_inc, libinc],
    dependencies : libcore_dep)
