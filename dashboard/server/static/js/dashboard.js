// Connect to WebSocket server
const socket = io();

// DOM elements
const speedValue = document.getElementById('speed-value');
const throttleValue = document.getElementById('throttle-value');
const batteryValue = document.getElementById('battery-value');
const tempValue = document.getElementById('temp-value');
const brakeValue = document.getElementById('brake-value');
const steeringValue = document.getElementById('steering-value');

const speedControl = document.getElementById('speed-control');
const steeringControl = document.getElementById('steering-control');
const brakeControl = document.getElementById('brake-control');
const speedDisplay = document.getElementById('speed-display');
const steeringDisplay = document.getElementById('steering-display');
const brakeDisplay = document.getElementById('brake-display');

const emergencyStopBtn = document.getElementById('emergency-stop');
const sendCommandsBtn = document.getElementById('send-commands');

// Camera elements
const cameraPanel = document.getElementById('camera-panel');
const cameraFeed = document.getElementById('camera-feed');
const cameraStatus = document.getElementById('camera-status');
const cameraToggle = document.getElementById('camera-toggle');
const cameraSnapshot = document.getElementById('camera-snapshot');
let cameraActive = true;

// Get simulation mode toggle and status
const simulationToggle = document.getElementById('simulation-toggle');
const simulationStatus = document.getElementById('simulation-status');

// Hide simulation control
simulationToggle.parentElement.parentElement.parentElement.style.display = 'none';

// Update displays when controls change
speedControl.addEventListener('input', () => {
    speedDisplay.textContent = speedControl.value;
});

steeringControl.addEventListener('input', () => {
    steeringDisplay.textContent = steeringControl.value;
});

brakeControl.addEventListener('input', () => {
    brakeDisplay.textContent = brakeControl.value;
});

// Helper function for sending commands
function sendCommand(componentType, componentName, commandName, value) {
    // Validate value is an integer
    if (!Number.isInteger(value)) {
        console.error(`Invalid value for ${componentName}.${commandName}: ${value}`);
        return Promise.reject(new Error('Value must be an integer'));
    }
    
    return fetch('/api/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            component_type: componentType,
            component_name: componentName,
            command_name: commandName,
            direct_value: value
        }),
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`Server returned ${response.status}: ${response.statusText}`);
        }
        return response.json();
    })
    .catch(error => {
        console.error(`Error sending ${componentName} command:`, error);
        throw error; // Re-throw for handling by caller
    });
}

// Emergency stop button
let emergencyActive = false;
emergencyStopBtn.addEventListener('click', () => {
    emergencyActive = !emergencyActive;
    
    if (emergencyActive) {
        emergencyStopBtn.textContent = 'RELEASE EMERGENCY STOP';
        emergencyStopBtn.style.backgroundColor = '#d13535';
    } else {
        emergencyStopBtn.textContent = 'EMERGENCY STOP';
        emergencyStopBtn.style.backgroundColor = '#ff4a4a';
    }
    
    // Send command using the correct format
    sendCommand('CONTROLS', 'EMERGENCY', 'STOP', emergencyActive ? 1 : 0);
});

// Send commands button
sendCommandsBtn.addEventListener('click', () => {
    // Send multiple commands - speed, steering, and brake
    sendCommand('MOTORS', 'MAIN_DRIVE', 'SPEED', parseInt(speedControl.value));
    sendCommand('CONTROLS', 'STEERING', 'ANGLE', parseInt(steeringControl.value));
    sendCommand('CONTROLS', 'BRAKE', 'PRESSURE', parseInt(brakeControl.value));
});

// Setup Chart.js
const ctx = document.getElementById('historyChart').getContext('2d');
const chart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            {
                label: 'Speed (km/h)',
                borderColor: 'rgb(75, 192, 192)',
                data: [],
                pointRadius: 0,
                borderWidth: 2
            },
            {
                label: 'Motor Temp (°C)',
                borderColor: 'rgb(255, 99, 132)',
                data: [],
                pointRadius: 0,
                borderWidth: 2
            },
            {
                label: 'Battery (V)',
                borderColor: 'rgb(54, 162, 235)',
                data: [],
                pointRadius: 0,
                borderWidth: 2
            }
        ]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: {
            duration: 0 // Disable animations for better performance
        },
        elements: {
            line: {
                tension: 0.2 // Slight curve for better visualization
            }
        },
        scales: {
            x: {
                display: true,
                title: {
                    display: true,
                    text: 'Time'
                },
                ticks: {
                    maxTicksLimit: 10, // Limit X-axis labels for readability
                    autoSkip: true
                }
            },
            y: {
                display: true,
                title: {
                    display: true,
                    text: 'Value'
                },
                beginAtZero: true
            }
        },
        plugins: {
            legend: {
                position: 'top'
            }
        }
    }
});

// Data structure to track timestamps and values
const chartData = {
    timestamps: [], // Array of unique timestamps
    // Object to hold values for each metric, indexed by timestamp
    speed: {},
    motor_temp: {},
    battery_voltage: {}
};

// Helper function to find or add timestamp in sorted array
function findOrAddTimestamp(timestamp) {
    const timeStr = new Date(timestamp * 1000).toLocaleTimeString();
    const index = chartData.timestamps.indexOf(timeStr);
    
    if (index !== -1) {
        return { index, timeStr };
    }
    
    // Add new timestamp in chronological order
    chartData.timestamps.push(timeStr);
    
    // Sort timestamps (ensures they remain in chronological order)
    chartData.timestamps.sort((a, b) => {
        const dateA = new Date(`1970-01-01 ${a}`);
        const dateB = new Date(`1970-01-01 ${b}`);
        return dateA - dateB;
    });
    
    return { 
        index: chartData.timestamps.indexOf(timeStr), 
        timeStr 
    };
}

// Helper function to update chart with current data
function updateChart() {
    // Reset chart data
    chart.data.labels = [...chartData.timestamps];
    
    // Update each dataset
    const speedData = chartData.timestamps.map(ts => chartData.speed[ts] || null);
    const tempData = chartData.timestamps.map(ts => chartData.motor_temp[ts] || null);
    const batteryData = chartData.timestamps.map(ts => chartData.battery_voltage[ts] || null);
    
    chart.data.datasets[0].data = speedData;
    chart.data.datasets[1].data = tempData;
    chart.data.datasets[2].data = batteryData;
    
    // Force complete redraw
    chart.update('none');
}

// Helper function to add or update a data point
function updateDataPoint(timestamp, metric, value) {
    const { timeStr } = findOrAddTimestamp(timestamp);
    
    // Update the specific metric at the timestamp
    if (metric === 'speed') {
        chartData.speed[timeStr] = value;
    } else if (metric === 'motor_temp') {
        chartData.motor_temp[timeStr] = value;
    } else if (metric === 'battery_voltage') {
        chartData.battery_voltage[timeStr] = value;
    }
    
    // Enforce data point limit (keep last 600 timestamps)
    if (chartData.timestamps.length > 600) {
        const removed = chartData.timestamps.shift();
        delete chartData.speed[removed];
        delete chartData.motor_temp[removed];
        delete chartData.battery_voltage[removed];
    }
}

// Fetch historical data on load
fetch('/api/telemetry/history')
    .then(response => response.json())
    .then(data => {
        if (data.length === 0) return;
        
        // Process each data point
        data.forEach(item => {
            // Update data points based on available metrics
            if ('speed' in item) {
                updateDataPoint(item.timestamp, 'speed', item.speed);
            }
            if ('motor_temp' in item) {
                updateDataPoint(item.timestamp, 'motor_temp', item.motor_temp);
            }
            if ('battery_voltage' in item) {
                updateDataPoint(item.timestamp, 'battery_voltage', item.battery_voltage);
            }
        });
        
        // Update chart with processed data
        updateChart();
        
        console.log(`Loaded ${data.length} historical data points`);
    })
    .catch(error => {
        console.error('Error fetching historical data:', error);
    });

// Listen for real-time updates
socket.on('state_update', (state) => {
    // Update dashboard metrics
    if ('speed' in state) {
        speedValue.textContent = parseFloat(state.speed).toFixed(2);
        updateDataPoint(state.timestamp, 'speed', state.speed);
    }
    
    if ('throttle' in state) {
        throttleValue.textContent = parseInt(state.throttle);
    }
    
    if ('battery_voltage' in state) {
        batteryValue.textContent = parseFloat(state.battery_voltage).toFixed(2);
        updateDataPoint(state.timestamp, 'battery_voltage', state.battery_voltage);
    }
    
    if ('motor_temp' in state) {
        tempValue.textContent = parseFloat(state.motor_temp).toFixed(2);
        updateDataPoint(state.timestamp, 'motor_temp', state.motor_temp);
    }
    
    if ('brake_pressure' in state) {
        brakeValue.textContent = parseFloat(state.brake_pressure).toFixed(2);
    }
    
    if ('steering_angle' in state) {
        steeringValue.textContent = parseFloat(state.steering_angle).toFixed(2);
    }
    
    // Update chart whenever we get a telemetry update
    if ('timestamp' in state && ('speed' in state || 'motor_temp' in state || 'battery_voltage' in state)) {
        updateChart();
    }
    
    // Update light controls
    if ('light_mode' in state) {
        lightModeButtons.forEach((btn, idx) => {
            btn.classList.toggle('active', idx === state.light_mode);
        });
    }
    
    if ('light_signal' in state) {
        signalButtons.forEach((btn, idx) => {
            btn.classList.toggle('active', idx === state.light_signal);
        });
    }
    
    if ('light_brake' in state) {
        brakeToggle.checked = state.light_brake === 1;
        brakeStatus.textContent = state.light_brake === 1 ? 'On' : 'Off';
    }
    
    if ('light_test' in state) {
        testToggle.checked = state.light_test === 1;
        testStatus.textContent = state.light_test === 1 ? 'On' : 'Off';
    }
});

// Check camera availability
fetch('/api/camera/status')
    .then(response => response.json())
    .then(data => {
        if (!data.available) {
            // Hide camera panel if camera is not available
            cameraPanel.style.display = 'none';
            console.log('Camera not available');
        } else {
            cameraStatus.textContent = 'Camera Ready';
            setTimeout(() => {
                cameraStatus.style.display = 'none';
            }, 3000);
            console.log('Camera available:', data);
        }
    })
    .catch(error => {
        console.error('Error checking camera status:', error);
        cameraPanel.style.display = 'none';
    });

// Handle camera controls
socket.on('camera_frame', (data) => {
    if (cameraActive) {
        cameraFeed.src = 'data:image/jpeg;base64,' + data.image;
        if (cameraStatus.style.display !== 'none') {
            cameraStatus.style.display = 'none';
        }
    }
});

cameraToggle.addEventListener('click', () => {
    cameraActive = !cameraActive;
    cameraToggle.textContent = cameraActive ? 'Pause Feed' : 'Resume Feed';
    
    if (!cameraActive) {
        // Show last frame but grayed out
        cameraFeed.style.filter = 'grayscale(100%)';
    } else {
        cameraFeed.style.filter = 'none';
    }
});

cameraSnapshot.addEventListener('click', () => {
    // Create a download link for the current frame
    const link = document.createElement('a');
    link.download = 'gokart-snapshot-' + new Date().toISOString().replace(/:/g, '-') + '.jpg';
    link.href = cameraFeed.src;
    link.click();
});

// Light control elements
const lightModeButtons = document.querySelectorAll('.light-mode-btn');
const signalButtons = document.querySelectorAll('.signal-btn');
const locationButtons = document.querySelectorAll('.location-btn');
const brakeToggle = document.getElementById('brake-toggle');
const brakeStatus = document.getElementById('brake-status');
const testToggle = document.getElementById('test-toggle');
const testStatus = document.getElementById('test-status');

function getLocation() {
    // findIndex is not a function on the locationButtons object
    const index = Array.from(locationButtons).findIndex(btn => btn.classList.contains('active'));
    // todo: make more generic for more locations to be added in the future
    return index === 0 ? 'FRONT' : 'REAR';
}

// Light mode control
lightModeButtons.forEach((button, index) => {
    button.addEventListener('click', () => {
        lightModeButtons.forEach(btn => btn.classList.remove('active'));
        button.classList.add('active');

        const mode = button.textContent.toLowerCase();
        const modeValue = mode === 'off'
            ? 0
            : mode === 'low'
                ? 1
                : mode === 'high'
                    ? 2
                    : mode === 'hazard' ? 8 : 0;

        fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'LIGHTS',
                component_name: getLocation(),
                command_name: 'MODE',
                direct_value: modeValue
            }),
        });
    });
});

// Turn signal control
signalButtons.forEach((button, index) => {
    button.addEventListener('click', () => {
        signalButtons.forEach(btn => btn.classList.remove('active'));
        button.classList.add('active');
        
        fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'LIGHTS',
                component_name: getLocation(),
                command_name: 'SIGNAL',
                direct_value: index
            }),
        });
    });
});

// Brake lights control
brakeToggle.addEventListener('change', () => {
    const isOn = brakeToggle.checked;
    brakeStatus.textContent = isOn ? 'On' : 'Off';

    // get location from location buttons
    
    fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'LIGHTS',
                component_name: getLocation(),
                command_name: 'BRAKE',
                direct_value: isOn ? 1 : 0
            }),
        });
});

// Test mode control
testToggle.addEventListener('change', () => {
    const isOn = testToggle.checked;
    testStatus.textContent = isOn ? 'On' : 'Off';
    
    fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'CONTROLS',
                component_name: 'DIAGNOSTIC',
                command_name: 'MODE',
                direct_value: isOn ? 1 : 0
            }),
        });
});

// Location control
locationButtons.forEach((button, index) => {
    button.addEventListener('click', () => {
        locationButtons.forEach(btn => btn.classList.remove('active'));
        button.classList.add('active');
        
        fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'LIGHTS',
                component_name: getLocation(),
                command_name: 'LOCATION',
                direct_value: index === 0 ? 0 : 1 // 0 for FRONT, 1 for REAR
            }),
        });
    });
}); 