#!/bin/sh
##
## Uses r35krag0th's fingerprint script to update the entry in the config file
##
## Author: Armen Kaleshian (kriation) <armen@kriation.com>

CONFIG="$HOME/.config/pianobar/config"

# Get Key
KEY=`openssl s_client -connect tuner.pandora.com:443 < /dev/null 2> /dev/null | openssl x509 -noout -fingerprint | tr -d ':' | cut -d'=' -f2`

# Replace key in config
sed -in "s/tls_fingerprint = \w*/tls_fingerprint = $KEY/g" $CONFIG
