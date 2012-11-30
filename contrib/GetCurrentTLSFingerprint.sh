#!/bin/bash
##
## A simple little shell script that will return the current
##  fingerprint on the SSL certificate.  It's crude but works :D
##
## Author: Bob Saska (r35krag0th) <git@r35.net>

openssl s_client -connect tuner.pandora.com:443 < /dev/null 2> /dev/null | \
    openssl x509 -noout -fingerprint | tr -d ':' | cut -d'=' -f2
