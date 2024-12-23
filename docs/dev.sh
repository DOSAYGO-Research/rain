#!/usr/bin/env bash

source ~/.nvm/nvm.sh

if ! command -v serve &>/dev/null; then
  npm i -g serve@latest
fi

serve --ssl-cert ~/sslcerts/fullchain.pem --ssl-key ~/sslcerts/privkey.pem -p 8080 docs/

