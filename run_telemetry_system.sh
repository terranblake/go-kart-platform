#!/bin/bash
# Run Telemetry System
# This script starts all components of the telemetry system in the correct order

# Set Python path to include project root
export PYTHONPATH=$PYTHONPATH:$(pwd)

echo "Starting telemetry system..."

# Kill any existing processes
pkill -f "collector.py" || true
pkill -f "simulate_can_data.py" || true
pkill -f "app.py" || true
rm telemetry.db || true

echo "Starting telemetry collector..."
# Start the collector in the background
python telemetry/collector/collector.py &
COLLECTOR_PID=$!
echo "Collector started with PID: $COLLECTOR_PID"

# Wait for collector to initialize
sleep 3

echo "Starting CAN simulator..."
# Start the CAN simulator in the background
python tools/diagnostics/simulate_can_data.py --pattern realistic --interval 0.2 &
SIMULATOR_PID=$!
echo "CAN simulator started with PID: $SIMULATOR_PID"

# Wait for simulator to initialize
sleep 2

echo "Starting dashboard server..."
# Start the dashboard server in the background
python dashboard/server/app.py &
DASHBOARD_PID=$!
echo "Dashboard server started with PID: $DASHBOARD_PID"

echo "All components started. Press Ctrl+C to stop all."
echo "Dashboard URL: http://localhost:5000"
echo "Collector API: http://localhost:5001"

# Wait for interrupt
trap "echo 'Stopping all components...'; kill $COLLECTOR_PID $SIMULATOR_PID $DASHBOARD_PID 2>/dev/null" INT
wait 