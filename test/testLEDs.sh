#!/bin/bash

_HOST="wifidemo.com"
_PORT=5001
_PATH="led"

_URL=${_HOST}:${_PORT}/${_PATH}

echo $x

# Cycle red LED light on then off
echo "Cycling RED led light"
curl -H "Content-Type: application/json" -X POST -d "{\"red\": \"on\"}" $_URL
sleep 1
curl -H "Content-Type: application/json" -X POST -d "{\"red\": \"off\"}" $_URL

# Cycle orange LED light on then off
echo "Cycling ORANGE led light"
curl -H "Content-Type: application/json" -X POST -d "{\"orange\": \"on\"}" $_URL
sleep 1
curl -H "Content-Type: application/json" -X POST -d "{\"orange\": \"off\"}" $_URL

# Cycle green LED light on then off
echo "Cycling GREEN led light"
curl -H "Content-Type: application/json" -X POST -d "{\"green\": \"on\"}" $_URL
sleep 1
curl -H "Content-Type: application/json" -X POST -d "{\"green\": \"off\"}" $_URL

# Cycle all three lights
echo "Cycling all three led light"
curl -H "Content-Type: application/json" -X POST -d "{\"red\": \"on\", \"orange\": \"on\", \"green\": \"on\"}" $_URL
sleep 1
curl -H "Content-Type: application/json" -X POST -d "{\"red\": \"off\", \"orange\": \"off\", \"green\": \"off\"}" $_URL

# Get status of the lights
echo "LED status:"
curl $_URL
echo ""
