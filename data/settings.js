let currentSlaveId = null;
let currentSlaveName = null;

function showStatus(message, type) {
    const status = document.getElementById('status');
    status.textContent = message;
    status.className = 'status-message ' + type;
    status.style.display = 'block';
    
    setTimeout(() => {
        status.style.display = 'none';
    }, 5000);
}

async function searchSlaveConfig() {
    const slaveId = document.getElementById('search_slave_id').value;
    const slaveName = document.getElementById('search_slave_name').value;

    if (!slaveId || !slaveName) {
        showStatus('Please enter both Slave ID and Slave Name', 'error');
        return;
    }

    try {
        showStatus('Searching for slave configuration...', 'success');
        
        // First, load all slaves to find the specific one
        const response = await fetch('/getslaves');
        if (!response.ok) {
            throw new Error('Failed to load slave configuration');
        }

        const config = await response.json();
        const slaves = config.slaves || [];
        
        // Find the specific slave
        const foundSlave = slaves.find(slave => 
            slave.id === parseInt(slaveId) && slave.name === slaveName
        );

        if (!foundSlave) {
            showStatus(`Slave ID ${slaveId} with name "${slaveName}" not found`, 'error');
            return;
        }

        // Store current slave info
        currentSlaveId = parseInt(slaveId);
        currentSlaveName = slaveName;

        // Display the configuration for editing
        const editor = document.getElementById('config_editor');
        editor.value = JSON.stringify(foundSlave, null, 2);
        
        // Show the edit section
        document.getElementById('editSection').style.display = 'block';
        
        showStatus(`Found slave configuration for ${slaveName} (ID: ${slaveId})`, 'success');
        
    } catch (error) {
        showStatus('Error searching for slave: ' + error.message, 'error');
        console.error('Search error:', error);
    }
}

async function saveEditedConfig() {
    if (!currentSlaveId || !currentSlaveName) {
        showStatus('No slave configuration loaded to save', 'error');
        return;
    }

    const editor = document.getElementById('config_editor');
    const editedConfig = editor.value;

    if (!editedConfig) {
        showStatus('Configuration is empty', 'error');
        return;
    }

    try {
        // Validate JSON
        const parsedConfig = JSON.parse(editedConfig);
        
        // Basic validation
        if (!parsedConfig.id || !parsedConfig.name) {
            showStatus('Configuration must include id and name fields', 'error');
            return;
        }

        // Load current slaves configuration
        const response = await fetch('/getslaves');
        if (!response.ok) {
            throw new Error('Failed to load current slave configuration');
        }

        const currentConfig = await response.json();
        const slaves = currentConfig.slaves || [];
        
        // Find and update the specific slave
        const slaveIndex = slaves.findIndex(slave => 
            slave.id === currentSlaveId && slave.name === currentSlaveName
        );

        if (slaveIndex === -1) {
            showStatus('Original slave configuration not found - it may have been modified', 'error');
            return;
        }

        // Replace the slave configuration
        slaves[slaveIndex] = parsedConfig;

        // Save updated configuration
        const saveResponse = await fetch('/saveslaves', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ slaves: slaves })
        });

        if (saveResponse.ok) {
            showStatus('Slave configuration updated successfully!', 'success');
            
            // Update current references
            currentSlaveId = parsedConfig.id;
            currentSlaveName = parsedConfig.name;
            
            // Clear search form
            document.getElementById('search_slave_id').value = '';
            document.getElementById('search_slave_name').value = '';
            
        } else {
            throw new Error('Failed to save configuration');
        }

    } catch (error) {
        if (error instanceof SyntaxError) {
            showStatus('Invalid JSON format. Please check your configuration.', 'error');
        } else {
            showStatus('Error saving configuration: ' + error.message, 'error');
        }
        console.error('Save error:', error);
    }
}

function cancelEdit() {
    document.getElementById('editSection').style.display = 'none';
    document.getElementById('config_editor').value = '';
    currentSlaveId = null;
    currentSlaveName = null;
    showStatus('Edit cancelled', 'success');
}

// Enter key support for search form
document.addEventListener('DOMContentLoaded', function() {
    const searchForm = document.getElementById('searchForm');
    searchForm.addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            e.preventDefault();
            searchSlaveConfig();
        }
    });
});