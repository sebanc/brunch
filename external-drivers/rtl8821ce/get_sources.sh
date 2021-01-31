#!/bin/bash

## This script gets the driver sources from Endless OS and bootstraps this repository
## Make sure to run this (cwd) from the root of the repository 

git clone --depth=1 git@github.com:endlessm/linux.git

cp -r linux/drivers/net/wireless/rtl8821ce/* .
