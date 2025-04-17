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
    sendCommand('MOTORS', 'MOTOR_LEFT_REAR', 'THROTTLE', parseInt(speedControl.value));
    sendCommand('CONTROLS', 'STEERING', 'ANGLE', parseInt(steeringControl.value));
    sendCommand('MOTORS', 'MOTOR_LEFT_REAR', 'BRAKE', parseInt(brakeControl.value) > 0 ? 1 : 0);
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
    speed: {},      // motor_main/rpm
    motor_temp: {}, // motor_main/temperature
    battery_voltage: {} // battery/voltage
};

// Mapping of component/command to chart properties
const dataMapping = {
    'sensors': {
        'motor_main': {
            'rpm': { metric: 'speed', conversion: val => parseInt(val) / 100 }, // Convert RPM to approximate speed
            'temperature': { metric: 'motor_temp', conversion: val => parseFloat(val) }
        },
        'battery': {
            'voltage': { metric: 'battery_voltage', conversion: val => parseFloat(val) }
        }
    }
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

// Helper function to add or update a data point based on component_type, component_id, and command_id
function updateDataPointFromState(state) {
    // Check if this state update is for a telemetry value we chart
    const componentMapping = dataMapping[state.component_type];
    if (!componentMapping) return false;
    
    const commandMapping = componentMapping[state.component_id];
    if (!commandMapping) return false;
    
    const valueMapping = commandMapping[state.command_id];
    if (!valueMapping) return false;
    
    // Convert string value to appropriate numeric type
    const value = valueMapping.conversion(state.value);
    
    // Find or add timestamp slot
    const { timeStr } = findOrAddTimestamp(state.timestamp);
    
    // Update the specific metric
    chartData[valueMapping.metric][timeStr] = value;
    
    // Enforce data point limit (keep last 600 timestamps)
    if (chartData.timestamps.length > 600) {
        const removed = chartData.timestamps.shift();
        delete chartData.speed[removed];
        delete chartData.motor_temp[removed];
        delete chartData.battery_voltage[removed];
    }
    
    return true;
}

// Fetch historical data on load
fetch('/api/telemetry/history')
    .then(response => response.json())
    .then(data => {
        if (data.length === 0) return;
        
        // Process each data point
        let dataUpdated = false;
        data.forEach(item => {
            if (updateDataPointFromState(item)) {
                dataUpdated = true;
            }
        });
        
        // Update chart if we processed any data
        if (dataUpdated) {
            updateChart();
        }
        
        console.log(`Loaded ${data.length} historical data points`);
    })
    .catch(error => {
        console.error('Error fetching historical data:', error);
    });

// Listen for real-time updates
socket.on('state_update', (state) => {
    // Update data point if it's a chartable metric
    let chartUpdated = updateDataPointFromState(state);
    
    // Update dashboard metrics based on the component_type, component_id, and command_id
    if (state.component_type === 'sensors') {
        if (state.component_id === 'motor_main') {
            if (state.command_id === 'rpm') {
                speedValue.textContent = (parseInt(state.value) / 100).toFixed(2); // Convert RPM to speed
            } else if (state.command_id === 'temperature') {
                tempValue.textContent = parseFloat(state.value).toFixed(2);
            } else if (state.command_id === 'throttle') {
                throttleValue.textContent = parseInt(state.value);
            }
        } else if (state.component_id === 'battery') {
            if (state.command_id === 'voltage') {
                batteryValue.textContent = parseFloat(state.value).toFixed(2);
            }
        } else if (state.component_id === 'brake') {
            if (state.command_id === 'pressure') {
                brakeValue.textContent = parseFloat(state.value).toFixed(2);
            }
        } else if (state.component_id === 'steering') {
            if (state.command_id === 'angle') {
                steeringValue.textContent = parseFloat(state.value).toFixed(2);
            }
        }
    } else if (state.component_type === 'lights') {
        const isRear = state.component_id === 'rear';
        const isFront = state.component_id === 'front';
        
        // Only handle light updates for the current active location
        const activeLocation = getLocation();
        if ((isFront && activeLocation === 'FRONT') || (isRear && activeLocation === 'REAR')) {
            if (state.command_id === 'mode') {
                const modeValue = parseInt(state.value);
                // Map mode values to button indices
                const buttonIndex = modeValue === 0 ? 0 : // Off
                                   modeValue === 1 ? 1 : // Low
                                   modeValue === 2 ? 2 : // High
                                   modeValue === 8 ? 3 : 0; // Hazard
                                   
                lightModeButtons.forEach((btn, idx) => {
                    btn.classList.toggle('active', idx === buttonIndex);
                });
            } else if (state.command_id === 'signal') {
                const signalValue = parseInt(state.value);
                signalButtons.forEach((btn, idx) => {
                    btn.classList.toggle('active', idx === signalValue);
                });
            } else if (state.command_id === 'brake') {
                const brakeValue = parseInt(state.value) === 1;
                brakeToggle.checked = brakeValue;
                brakeStatus.textContent = brakeValue ? 'On' : 'Off';
            }
        }
    } else if (state.component_type === 'controls' && state.component_id === 'diagnostic' && state.command_id === 'mode') {
        const testValue = parseInt(state.value) === 1;
        testToggle.checked = testValue;
        testStatus.textContent = testValue ? 'On' : 'Off';
    }
    
    // Update chart if necessary
    if (chartUpdated) {
        updateChart();
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
                    ? 4
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