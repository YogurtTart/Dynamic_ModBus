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
        
        // Search for specific slave using new endpoint
        const response = await fetch('/getslaveconfig', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                slaveId: parseInt(slaveId),
                slaveName: slaveName
            })
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || 'Failed to find slave configuration');
        }

        const slaveConfig = await response.json();

        // Store current slave info
        currentSlaveId = parseInt(slaveId);
        currentSlaveName = slaveName;

        // Display the configuration for editing
        const editor = document.getElementById('config_editor');
        editor.value = JSON.stringify(slaveConfig, null, 2);
        
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

        // Ensure we're updating the correct slave
        if (parsedConfig.id !== currentSlaveId || parsedConfig.name !== currentSlaveName) {
            showStatus('Cannot change Slave ID or Name during edit', 'error');
            return;
        }

        // Save the updated configuration using new endpoint
        const saveResponse = await fetch('/updateslaveconfig', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(parsedConfig)
        });

        if (saveResponse.ok) {
            const result = await saveResponse.json();
            showStatus(result.message || 'Slave configuration updated successfully!', 'success');
            
            // Clear search form
            document.getElementById('search_slave_id').value = '';
            document.getElementById('search_slave_name').value = '';
            
            // Hide edit section
            document.getElementById('editSection').style.display = 'none';
            
        } else {
            const errorData = await saveResponse.json();
            throw new Error(errorData.message || 'Failed to save configuration');
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