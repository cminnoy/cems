# Home Energy Management System

This repo contains components that read out different meters and control the batteries.

Information is exposed to Home Assistant via MQTT for logging and monitoring.
Information is exposed to Graphite/Whisper to display in Grafana.

The components communicate with each other using shared memory (Linux) using the Fabrix framework.

