#!/bin/bash
##
## A simple little shell script that will return the current
##  fingerprint on the SSL certificate.  It's crude but works :D
##
## Author: Bob Saska (r35krag0th) <git@r35.net>

echo |\
openssl s_client -connect tuner.pandora.com:443 2>&1 |\
sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' |\
openssl x509 -noout -fingerprint |\
sed 's/://g' |\
cut -d= -f2
