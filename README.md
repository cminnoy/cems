# Home Energy Management System

This repo contains some components of my home energy management system.
It reads out different meters and controls two Zendure 2400AC batteries.
The components communicate with each other using the Fabrix framework.

Information is exposed to Home Assistant via MQTT for logging and monitoring using a mqtt bridge.
Information is exposed to Graphite/Whisper to display in Grafana using the graphite logger.

