// Protocol Documentation JavaScript

document.addEventListener('DOMContentLoaded', () => {
    // Initial setup
    console.log('DOM loaded, initializing protocol page');
    document.getElementById('loading').style.display = 'flex';
    fetchProtocolStructure();
    setupNavigation();
});

// Fetch the complete protocol structure from the API
async function fetchProtocolStructure() {
    try {
        console.log('Fetching protocol structure...');
        const response = await fetch('/api/protocol');
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        
        console.log('Response received, parsing JSON...');
        const data = await response.json();
        
        // Debug: Log the response from the API
        console.log('Protocol data received:', data);
        console.log('MessageTypes:', data.message_types);
        console.log('ComponentTypes:', data.component_types);
        console.log('ValueTypes:', data.value_types);
        console.log('Components:', data.components);
        console.log('Component keys:', Object.keys(data.components || {}));
        
        if (!data.message_types || !data.component_types || !data.value_types || !data.components) {
            console.error('Missing expected data in protocol response:', {
                hasMessageTypes: !!data.message_types,
                hasComponentTypes: !!data.component_types,
                hasValueTypes: !!data.value_types,
                hasComponents: !!data.components
            });
            throw new Error('Protocol data incomplete');
        }
        
        // Process and display the protocol data
        console.log('Displaying protocol data...');
        displayProtocolData(data);
        
        console.log('Setting up test interface...');
        setupTestInterface(data);
        
        // Hide loading, show content - using direct style manipulation to ensure it works
        console.log('Hiding loading spinner, showing content...');
        const loadingElement = document.getElementById('loading');
        if (loadingElement) {
            loadingElement.style.display = 'none';
            console.log('Loading element hidden');
        } else {
            console.error('Loading element not found');
        }
        
        // Show all other sections
        const sections = [
            'protocol-overview',
            'message-types',
            'component-types',
            'value-types',
            'components-section',
            'test-interface'
        ];
        
        sections.forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.classList.remove('hidden');
                element.style.display = 'block';
                console.log(`Section ${id} shown`);
            } else {
                console.error(`Section element ${id} not found`);
            }
        });
        
        console.log('Setting up collapsible sections...');
        setupCollapsibleSections();
        
    } catch (error) {
        console.error('Error fetching protocol data:', error);
        const loadingElement = document.getElementById('loading');
        if (loadingElement) {
            loadingElement.innerHTML = `
                <div class="error">
                    <h3>Error Loading Protocol Data</h3>
                    <p>${error.message}</p>
                    <button onclick="location.reload()">Retry</button>
                </div>
            `;
        }
    }
}

// Process and display the protocol data in appropriate sections
function displayProtocolData(data) {
    // Message Types
    populateEnumTable('message-types-table', data.message_types);
    
    // Component Types
    populateEnumTable('component-types-table', data.component_types);
    
    // Value Types
    populateEnumTable('value-types-table', data.value_types);
    
    // Components and Commands
    displayComponents(data.components);
    
    // Update navigation with actual component types
    updateNavigation(data);
    
    // Add click handlers to section headers to toggle content visibility
    setupCollapsibleSections();
}

// Populate enum tables (message_types, component_types, value_types)
function populateEnumTable(tableId, enumData) {
    const tableBody = document.getElementById(tableId);
    if (!tableBody) return;
    
    // Clear existing content
    tableBody.innerHTML = '';
    
    // Add enum rows
    for (const key in enumData) {
        const value = enumData[key];
        const row = document.createElement('tr');
        
        row.innerHTML = `
            <td>${key}</td>
            <td>${value}</td>
            <td>${getEnumDescription(key)}</td>
        `;
        
        tableBody.appendChild(row);
    }
}

// Get description for enum values (this could be enhanced with actual descriptions)
function getEnumDescription(enumKey) {
    // This is a simple placeholder. In a real implementation, you'd provide actual descriptions
    // for each enum value, possibly fetched from the API or stored in a configuration.
    const descriptions = {
        // Message Types
        'COMMAND': 'Sent to command a component to perform an action',
        'STATUS': 'Sent by a component to report its current status',
        'TELEMETRY': 'Periodic data reporting from components',
        'ERROR': 'Error reporting from components',
        
        // Component Types
        'LIGHTS': 'Lighting systems on the go-kart',
        'MOTORS': 'Drive motors and related systems',
        'SENSORS': 'Various sensors for environmental and vehicle data',
        'CONTROLS': 'Steering, throttle, and braking systems',
        'BATTERY': 'Battery and power management systems',
        
        // Value Types
        'BOOL': 'Boolean value (true/false)',
        'UINT8': 'Unsigned 8-bit integer (0-255)',
        'INT8': 'Signed 8-bit integer (-128 to 127)',
        'UINT16': 'Unsigned 16-bit integer (0-65535)',
        'INT16': 'Signed 16-bit integer (-32768 to 32767)',
        'UINT32': 'Unsigned 32-bit integer (0-4294967295)',
        'INT32': 'Signed 32-bit integer (-2147483648 to 2147483647)',
        'FLOAT': 'Single-precision 32-bit floating point'
    };
    
    return descriptions[enumKey] || 'No description available';
}

// Display components and their commands
function displayComponents(components) {
    const container = document.getElementById('components-container');
    if (!container) return;
    
    // Clear existing content
    container.innerHTML = '';
    
    // Process each component type section
    for (const componentType in components) {
        const typeSection = document.createElement('div');
        typeSection.className = 'component-type-section';
        typeSection.id = componentType.toLowerCase();
        
        // Convert component type to uppercase for display
        const displayComponentType = componentType.toUpperCase();
        typeSection.innerHTML = `<h3>${displayComponentType}</h3>`;
        
        // Add each component in this type
        const componentsOfType = components[componentType];
        
        for (const componentName in componentsOfType) {
            const componentData = componentsOfType[componentName];
            const componentCard = createComponentCard(componentName, componentData);
            typeSection.appendChild(componentCard);
        }
        
        container.appendChild(typeSection);
    }
}

// Create a card for a single component
function createComponentCard(componentName, componentData) {
    const card = document.createElement('div');
    card.className = 'component-card';
    card.id = `component-${componentName.toLowerCase().replace(/\s+/g, '-')}`;
    
    const header = document.createElement('div');
    header.className = 'component-header';
    header.innerHTML = `
        <span>${componentName}</span>
        <span>ID: ${componentData.component_id}</span>
    `;
    
    const body = document.createElement('div');
    body.className = 'component-body';
    
    // Component description
    const description = document.createElement('div');
    description.className = 'component-description';
    description.textContent = componentData.description || `${componentName} component`;
    
    // Command table
    const commandTableContainer = document.createElement('div');
    commandTableContainer.className = 'command-table-container';
    
    const commandTable = document.createElement('table');
    commandTable.className = 'command-table';
    commandTable.innerHTML = `
        <thead>
            <tr>
                <th>Command</th>
                <th>ID</th>
                <th>Value Type</th>
                <th>Description</th>
            </tr>
        </thead>
        <tbody></tbody>
    `;
    
    const tableBody = commandTable.querySelector('tbody');
    
    // Add each command to the table
    for (const commandName in componentData.commands) {
        const command = componentData.commands[commandName];
        const row = document.createElement('tr');
        
        row.innerHTML = `
            <td>${commandName}</td>
            <td>${command.command_id}</td>
            <td>${command.value_type}</td>
            <td>${command.description || 'No description'}</td>
        `;
        
        tableBody.appendChild(row);
    }
    
    commandTableContainer.appendChild(commandTable);
    
    // Assemble the component card
    body.appendChild(description);
    body.appendChild(commandTableContainer);
    
    card.appendChild(header);
    card.appendChild(body);
    
    return card;
}

// Update the navigation sidebar with actual component types
function updateNavigation(data) {
    const nav = document.getElementById('protocol-nav');
    if (!nav) return;
    
    // Get all li elements that have a class of nav-header
    const componentHeader = Array.from(nav.querySelectorAll('li.nav-header')).find(
        li => li.textContent === 'Components'
    );
    
    if (!componentHeader) return;
    
    // Remove all li elements after the Components header
    let nextSibling = componentHeader.nextElementSibling;
    while (nextSibling) {
        const current = nextSibling;
        nextSibling = current.nextElementSibling;
        nav.removeChild(current);
    }
    
    // Add component types from the data
    const componentTypes = Object.keys(data.component_types);
    componentTypes.forEach(type => {
        const li = document.createElement('li');
        const a = document.createElement('a');
        
        a.href = `#${type.toLowerCase()}`;
        a.textContent = type;
        
        li.appendChild(a);
        nav.appendChild(li);
    });
}

// Setup click events for navigation
function setupNavigation() {
    // Handle navigation clicks
    document.getElementById('protocol-nav').addEventListener('click', (event) => {
        if (event.target.tagName === 'A') {
            // Prevent default behavior if it's an anchor link
            if (event.target.getAttribute('href').startsWith('#')) {
                event.preventDefault();
                
                // Get the target section ID
                const targetId = event.target.getAttribute('href').substring(1);
                const targetElement = document.getElementById(targetId);
                
                // Scroll to the target section
                if (targetElement) {
                    window.scrollTo({
                        top: targetElement.offsetTop - 100,
                        behavior: 'smooth'
                    });
                }
            }
        }
    });
}

// Setup the test interface for sending commands
function setupTestInterface(protocolData) {
    console.log('Setting up test interface with data:', protocolData);
    
    const componentTypeSelect = document.getElementById('test-component-type');
    const componentSelect = document.getElementById('test-component-id');
    const commandSelect = document.getElementById('test-command-id');
    const valueInput = document.getElementById('test-value-enum');
    const sendButton = document.getElementById('test-send-command');
    const resultDiv = document.getElementById('test-result');
    
    if (!componentTypeSelect) {
        console.error('Missing component type select element');
        return;
    }
    
    // Clear existing options first
    componentTypeSelect.innerHTML = '';
    
    console.log('Available component types:', protocolData.component_types);
    
    // Populate component type dropdown
    for (const type in protocolData.component_types) {
        console.log(`Adding component type option: ${type}`);
        const option = document.createElement('option');
        option.value = type;
        option.textContent = type;
        componentTypeSelect.appendChild(option);
    }
    
    console.log(`Added ${componentTypeSelect.options.length} component type options`);
    
    // Update components when component type changes
    componentTypeSelect.addEventListener('change', () => {
        console.log(`Component type changed to: ${componentTypeSelect.value}`);
        updateComponentOptions(componentTypeSelect.value, protocolData);
    });
    
    // Update commands when component changes
    componentSelect.addEventListener('change', () => {
        console.log(`Component changed to: ${componentSelect.value}`);
        updateCommandOptions(componentSelect.value, protocolData);
    });
    
    // Update value input when command changes
    commandSelect.addEventListener('change', () => {
        console.log(`Command changed to: ${commandSelect.value}`);
        updateValueInput(
            componentSelect.value, 
            commandSelect.value, 
            protocolData
        );
    });
    
    // Initialize with first values if options are available
    if (componentTypeSelect.options.length > 0) {
        console.log('Initializing with first component type');
        componentTypeSelect.dispatchEvent(new Event('change'));
    } else {
        console.warn('No component type options to initialize with');
    }
    
    // Handle send button click
    sendButton.addEventListener('click', () => {
        sendTestCommand(
            componentSelect.value,
            commandSelect.value,
            getSelectedValue(valueInput),
            resultDiv
        );
    });
}

// Update component options based on selected component type
function updateComponentOptions(componentType, protocolData) {
    console.log(`Updating component options for type: ${componentType}`);
    
    const componentSelect = document.getElementById('test-component-id');
    
    if (!componentSelect) {
        console.error('Component select element not found');
        return;
    }
    
    // Clear existing options
    componentSelect.innerHTML = '';
    
    // Convert component type to lowercase to match the protocolData structure
    const componentTypeKey = componentType.toLowerCase();
    
    console.log(`Looking for components with type key: ${componentTypeKey}`);
    console.log('Available component types in data:', Object.keys(protocolData.components || {}));
    
    // Check if this component type exists in the data
    if (protocolData.components[componentTypeKey]) {
        console.log(`Found components for type ${componentTypeKey}:`, Object.keys(protocolData.components[componentTypeKey]));
        
        // Add components that match the selected type
        for (const component in protocolData.components[componentTypeKey]) {
            console.log(`Adding component option: ${component}`);
            const option = document.createElement('option');
            option.value = component;
            option.textContent = component;
            componentSelect.appendChild(option);
        }
        
        console.log(`Added ${componentSelect.options.length} component options`);
    } else {
        console.warn(`No components found for type: ${componentTypeKey}`);
    }
    
    // Trigger change event to update commands
    if (componentSelect.options.length > 0) {
        console.log('Triggering component change event');
        componentSelect.dispatchEvent(new Event('change'));
    } else {
        console.warn('No component options to select');
    }
}

// Update command options based on selected component
function updateCommandOptions(componentName, protocolData) {
    console.log(`Updating command options for component: ${componentName}`);
    
    const commandSelect = document.getElementById('test-command-id');
    const componentTypeSelect = document.getElementById('test-component-type');
    const componentType = componentTypeSelect.value.toLowerCase();
    
    if (!commandSelect) {
        console.error('Command select element not found');
        return;
    }
    
    // Clear existing options
    commandSelect.innerHTML = '';
    
    console.log(`Looking for commands in component type: ${componentType}, component: ${componentName}`);
    
    // Get commands for the selected component
    const component = protocolData.components[componentType] && 
                     protocolData.components[componentType][componentName];
    
    if (!component) {
        console.error(`Component not found: ${componentType}.${componentName}`);
        console.log('Available components:', 
            protocolData.components[componentType] ? 
            Object.keys(protocolData.components[componentType]) : 
            'None');
        return;
    }
    
    console.log('Component data:', component);
    console.log('Commands available:', component.commands ? Object.keys(component.commands) : 'None');
    
    // Add command options
    for (const command in component.commands) {
        console.log(`Adding command option: ${command}`);
        const option = document.createElement('option');
        option.value = command;
        option.textContent = command;
        commandSelect.appendChild(option);
    }
    
    console.log(`Added ${commandSelect.options.length} command options`);
    
    // Trigger change event to update value input
    if (commandSelect.options.length > 0) {
        console.log('Triggering command change event');
        commandSelect.dispatchEvent(new Event('change'));
    } else {
        console.warn('No command options to select');
    }
}

// Update value input based on selected command
function updateValueInput(componentName, commandName, protocolData) {
    console.log(`Updating value options for component: ${componentName}, command: ${commandName}`);
    
    const valueSelect = document.getElementById('test-value-enum');
    const componentTypeSelect = document.getElementById('test-component-type');
    const componentType = componentTypeSelect.value.toLowerCase();
    
    if (!valueSelect) {
        console.error('Value select element not found');
        return;
    }
    
    // Clear existing options
    valueSelect.innerHTML = '';
    
    console.log(`Looking for values in component type: ${componentType}, component: ${componentName}, command: ${commandName}`);
    
    // Get commands for the selected component
    const component = protocolData.components[componentType] && 
                     protocolData.components[componentType][componentName];
    
    if (!component) {
        console.error(`Component not found: ${componentType}.${componentName}`);
        return;
    }
    
    const command = component.commands && component.commands[commandName];
    if (!command) {
        console.error(`Command not found: ${componentType}.${componentName}.${commandName}`);
        return;
    }
    
    console.log('Command data:', command);
    console.log('Values available:', command.values ? Object.keys(command.values) : 'None');
    
    // If the command has predefined values, add them to the dropdown
    if (command.values && Object.keys(command.values).length > 0) {
        // Correctly iterate through the values object
        for (const [name, value] of Object.entries(command.values)) {
            console.log(`Adding value option: ${name} (${value})`);
            const option = document.createElement('option');
            option.value = value;
            option.textContent = name;
            valueSelect.appendChild(option);
        }
    } else {
        // If no enum values, add generic numeric options (0-10)
        console.log('No enum values, adding numeric options');
        for (let i = 0; i <= 10; i++) {
            const option = document.createElement('option');
            option.value = i;
            option.textContent = i.toString();
            valueSelect.appendChild(option);
        }
    }
    
    console.log(`Added ${valueSelect.options.length} value options`);
}

// Get selected value from the dropdown
function getSelectedValue(valueInput) {
    // Parse as integer to ensure we return a number, not a string
    return parseInt(valueInput.value, 10);
}

// Send test command to the API
async function sendTestCommand(componentName, commandName, value, resultDiv) {
    const componentTypeSelect = document.getElementById('test-component-type');
    const componentType = componentTypeSelect.value;
    
    console.log(`Sending test command: ${componentType}.${componentName}.${commandName} = ${value}`);
    
    try {
        resultDiv.innerHTML = `<p>Sending command...</p>`;
        
        const response = await fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                component_type: componentType,
                component_name: componentName,
                command_name: commandName,
                value: value
            })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        
        const result = await response.json();
        console.log('Command result:', result);
        
        resultDiv.innerHTML = `
            <h4>Command Sent</h4>
            <p>${componentType}.${componentName}.${commandName} = ${value}</p>
            <pre>${JSON.stringify(result, null, 2)}</pre>
        `;
        
    } catch (error) {
        console.error('Error sending command:', error);
        resultDiv.innerHTML = `
            <h4>Error</h4>
            <p>${error.message}</p>
        `;
    }
}

// Add collapsible behavior to sections
function setupCollapsibleSections() {
    console.log('Setting up collapsible sections with simplified approach...');
    
    // Sections to make collapsible (removed protocol-overview from this list)
    const collapsibleSections = [
        'message-types',
        'component-types',
        'value-types',
        'components-section'
    ];
    
    // First, make sure Protocol Overview is fully visible
    const overviewSection = document.getElementById('protocol-overview');
    if (overviewSection) {
        console.log('Making protocol-overview always visible');
        overviewSection.style.display = 'block';
        const overviewHeader = overviewSection.querySelector('h2');
        if (overviewHeader) {
            // Remove any collapsible styling
            overviewHeader.style.cursor = 'default';
            overviewHeader.style.paddingRight = '0';
        }
    }
    
    // Set up collapsible behavior for the other sections
    collapsibleSections.forEach(sectionId => {
        const section = document.getElementById(sectionId);
        if (!section) {
            console.error(`Section ${sectionId} not found`);
            return;
        }
        
        const header = section.querySelector('h2');
        if (!header) {
            console.error(`Header for section ${sectionId} not found`);
            return;
        }
        
        console.log(`Setting up collapsible for section: ${sectionId}`);
        
        // Create a wrapper div for the content
        const contentWrapper = document.createElement('div');
        contentWrapper.id = `${sectionId}-content`;
        contentWrapper.className = 'section-content';
        
        // Move all content after the header into the wrapper
        let nextElement = header.nextElementSibling;
        while (nextElement) {
            const elementToMove = nextElement;
            nextElement = nextElement.nextElementSibling;
            contentWrapper.appendChild(elementToMove);
        }
        
        // Add the wrapper back to the section
        section.appendChild(contentWrapper);
        
        // Style the header to look clickable
        header.style.cursor = 'pointer';
        header.style.position = 'relative';
        header.style.paddingRight = '30px';
        
        // Add arrow indicator
        const arrow = document.createElement('span');
        arrow.className = 'section-arrow';
        arrow.textContent = '►'; // Collapsed arrow
        arrow.style.position = 'absolute';
        arrow.style.right = '10px';
        arrow.style.top = '50%';
        arrow.style.transform = 'translateY(-50%)';
        
        header.appendChild(arrow);
        
        // Initially collapse all sections except test-interface
        contentWrapper.style.display = 'none';
        
        // Add click handler
        header.addEventListener('click', function() {
            console.log(`Section ${sectionId} header clicked`);
            
            // Toggle display
            if (contentWrapper.style.display === 'none') {
                contentWrapper.style.display = 'block';
                arrow.textContent = '▼'; // Expanded arrow
                console.log(`Section ${sectionId} expanded`);
            } else {
                contentWrapper.style.display = 'none';
                arrow.textContent = '►'; // Collapsed arrow
                console.log(`Section ${sectionId} collapsed`);
            }
        });
        
        console.log(`Collapsible setup complete for section: ${sectionId}`);
    });
    
    // Always show the test interface
    const testInterface = document.getElementById('test-interface');
    if (testInterface) {
        console.log('Ensuring test interface is visible');
        testInterface.style.display = 'block';
    }
} 