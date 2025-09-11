#!/bin/sh
strip cmd/_*
echo "# " > misc/changelog
echo "# This file contains the last 5 GIT commits." >> misc/changelog
echo "# " >> misc/changelog
git log -n 5 --pretty=format:"Author: %an%nMessage: %s%n" >> misc/changelog
