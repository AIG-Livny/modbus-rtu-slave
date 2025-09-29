#!/usr/bin/env python

from mapyr import *

def get_project(name:str) -> 'ProjectBase':
    if 'test' in name:
        import tests.mapyrfile
        return tests.mapyrfile.get_project(name)


    debug = 'debug' in name

    cfg = c.Config()
    cfg.COMPILER = 'gcc'
    cfg.AR = 'ar'

    cfg.INCLUDE_DIRS = ['src','include']

    if debug:
        cfg.CFLAGS.extend(['-g','-O0'])
    else:
        cfg.CFLAGS.extend(['-O3'])

    project = c.Project('main','bin/libmodbus-rtu-slave.a',cfg)

    c.add_default_rules(project)

    return project

if __name__ == "__main__":
    process(get_project)
