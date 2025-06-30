# Capstone Project: Hydroponic IoT Monitoring System

## Student Information
- **Name**: Hiten Shah
- **Student ID**: 20078332
- **Email**: hcshah26@hotmail.com / 20078332@tafe.wa.edu.au

## Overview
This project simulates and implements a hydroponic water monitoring and control system using an ESP32-S2 board, MQTT communication, and the Adafruit IO cloud platform. The system tracks inflow and outflow water levels, detects low/critical reservoir conditions, and prevents overflow — all remotely controlled via a dashboard.

## Features
- Real-time monitoring of water flow
- Digital float switch alerts (low, critical, overflow)
- MQTT communication with Adafruit IO
- Dashboard for control and visualisation
- Pump override (manual and remote)
- Fully simulated via Wokwi

## Key Components
- **Microcontroller**: ESP32-S2 (simulated)
- **Sensors**: 2 flowmeters, 3 float switches, 1 pump switch
- **Visual Indicators**: LEDs for pump and alerts
- **Cloud Platform**: Adafruit IO

## Dashboard Feeds
- `pump-in-flow-rate`
- `pump-return-flow-rate`
- `reservoir-warning-alert`
- `reservoir-critical-alert`
- `pipe-overflow-warning`
- `pump-override`

## Repository Contents
- `main.ino`: Main program code
- `README.md`: This file
- `docs/`: Reports, presentation, BOM, diagrams

## Related Links
- **GitHub Repository**: https://github.com/HCShah26/Capstone-IoT-Hydroponic.git
- **Wokwi Simulation**: https://wokwi.com/projects/433543810637673473
- **Adafruit IO Dashboard**: https://io.adafruit.com/hcshah26/dashboards/hydroponic-project

## License
This project is for educational use under ICT50220 Diploma of IT. No commercial use permitted.
