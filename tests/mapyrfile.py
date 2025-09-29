#!/usr/bin/env python

from mapyr import *

def get_project(name:str) -> 'ProjectBase':
    debug = 'debug' in name

    cfg = c.Config()
    cfg.COMPILER = 'gcc'
    cfg.AR = 'ar'

    cfg.INCLUDE_DIRS = ['src','../include']

    if debug:
        cfg.CFLAGS.extend(['-g','-O0'])
    else:
        cfg.CFLAGS.extend(['-O3'])

    cfg.LIBS = ['stdc++','gtest','m']

    project = c.Project('test','bin/test',cfg)

    libprj = utils.get_module('../mapyrfile.py').get_project('debug' if debug else 'release')
    project.subprojects.append(libprj)

    c.add_default_rules(project)

    return project

if __name__ == "__main__":
    process(get_project)
