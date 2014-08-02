#!/usr/bin/python2.7
import os, sys

COMPILER    = "gcc"
SRC_DIR     = "../src"
INCLUDE_DIR = "../src"
BIN_DIR     = "../bin"
BIN_NAME    = False
CFLAGS      = ["-O3", "-Wall", "-Wextra", "--std=c89", "-pedantic"]
DLIBS       = ["ws2_32"] if os.name == "nt" else []
DEFINES     = []


def strformat(fmt, var):
  for k in var:
    fmt = fmt.replace("{%s}" % str(k), var[k])
  return fmt


def listdir(path):
  return [os.path.join(dp, f) for dp, dn, fn in os.walk(path) for f in fn]


def main():
  os.chdir(sys.path[0])

  if len(sys.argv) < 2:
    print "usage: build.py c_file"
    sys.exit()

  global BIN_NAME
  if not BIN_NAME:
    BIN_NAME = sys.argv[1].replace(".c", ".exe" if os.name == "nt" else "")

  if not os.path.exists(BIN_DIR):
    os.makedirs(BIN_DIR)

  cfiles = filter(lambda x:x.endswith((".c", ".C")), listdir(SRC_DIR))
  cfiles.append(sys.argv[1])

  cmd = strformat(
    "{compiler} {flags} {include} {def} -o {outfile} {srcfiles} {libs} {argv}",
    {
      "compiler"  : COMPILER,
      "flags"     : " ".join(CFLAGS),
      "include"   : "-I" + INCLUDE_DIR,
      "def"       : " ".join(map(lambda x: "-D " + x, DEFINES)),
      "outfile"   : BIN_DIR + "/" + BIN_NAME,
      "srcfiles"  : " ".join(cfiles),
      "libs"      : " ".join(map(lambda x: "-l" + x, DLIBS)),
      "argv"      : " ".join(sys.argv[2:])
    })

  print "compiling..."
  res = os.system(cmd)

  if not res:
    print(BIN_DIR + "/" + BIN_NAME)

  print("done" + (" with errors" if res else ""))



if __name__ == "__main__":
  main()
