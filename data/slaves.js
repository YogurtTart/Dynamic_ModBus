let slaves = [];
let pollInterval = 10;

function showStatus(message, type) {
    const status = document.getElementById('status');
    status.textContent = message;
    status.className = 'status-message ' + type;
    status.style.display = 'block';
    
    setTimeout(() => {
        status.style.display = 'none';
    }, 5000);
}

function addSlave() {
    const slaveId = document.getElementById('slave_id').value;
    const startReg = document.getElementById('start_reg').value;
    const numReg = document.getElementById('num_reg').value;
    const slaveName = document.getElementById('slave_name').value;
    const mqttTopic = document.getElementById('mqtt_topic').value;

    if (!slaveId || !startReg || !numReg || !slaveName || !mqttTopic) {
        showStatus('Please fill all fields', 'error');
        return;
    }

    const slave = {
        id: parseInt(slaveId),
        startReg: parseInt(startReg),
        numReg: parseInt(numReg),
        name: slaveName,
        mqttTopic: mqttTopic
    };

    slaves.push(slave);
    updateSlavesList();
    clearSlaveForm();
    showStatus('Modbus slave added successfully!', 'success');
}

function clearSlaveForm() {
    document.getElementById('slave_id').value = '';
    document.getElementById('start_reg').value = '';
    document.getElementById('num_reg').value = '';
    document.getElementById('slave_name').value = '';
    document.getElementById('mqtt_topic').value = '';
}

function updateSlavesList() {
    const list = document.getElementById('slavesTableBody');
    const emptyState = document.getElementById('emptySlavesState');
    const slaveCount = document.getElementById('slaveCount');
    const slaveCountBadge = document.getElementById('slaveCountBadge');
    
    slaveCount.textContent = slaves.length;
    slaveCountBadge.textContent = slaves.length;
    
    if (slaves.length === 0) {
        list.innerHTML = '';
        emptyState.style.display = 'block';
        return;
    }
    
    emptyState.style.display = 'none';
    
    list.innerHTML = slaves.map((slave, index) => `
        <tr>
            <td><strong>${slave.id}</strong></td>
            <td>${slave.name}</td>
            <td>${slave.startReg}</td>
            <td>${slave.numReg}</td>
            <td><code>${slave.mqttTopic}</code></td>
            <td>
                <button class="btn btn-small btn-warning" onclick="deleteSlave(${index})" title="Delete slave">
                    🗑️ Delete
                </button>
            </td>
        </tr>
    `).join('');
}

function deleteSlave(index) {
    if (confirm('Are you sure you want to delete this Modbus slave?')) {
        slaves.splice(index, 1);
        updateSlavesList();
        showStatus('Modbus slave deleted successfully!', 'success');
    }
}

function savePollInterval() {
    const interval = document.getElementById('poll_interval').value;
    if (!interval || interval < 1) {
        showStatus('Please enter a valid polling interval', 'error');
        return;
    }
    pollInterval = parseInt(interval);
    document.getElementById('currentPollInterval').textContent = pollInterval + 's';
    showStatus(`Global poll interval set to ${pollInterval} seconds`, 'success');
}

async function saveSlaveConfig() {
    const config = {
        slaves: slaves,
        pollInterval: pollInterval
    };

    try {
        const response = await fetch('/saveslaves', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(config)
        });
        
        if (response.ok) {
            showStatus('Modbus configuration saved successfully!', 'success');
        } else {
            showStatus('Error saving configuration', 'error');
        }
    } catch (error) {
        showStatus('Error saving configuration: ' + error, 'error');
    }
}

async function loadSlaveConfig() {
    try {
        const response = await fetch('/getslaves');
        if (response.ok) {
            const config = await response.json();
            slaves = config.slaves || [];
            pollInterval = config.pollInterval || 10;
            
            document.getElementById('poll_interval').value = pollInterval;
            document.getElementById('currentPollInterval').textContent = pollInterval + 's';
            updateSlavesList();
            showStatus('Modbus configuration loaded successfully!', 'success');
        } else {
            showStatus('Error loading configuration', 'error');
        }
    } catch (error) {
        showStatus('Error loading configuration: ' + error, 'error');
    }
}

// Load configuration when page loads
document.addEventListener('DOMContentLoaded', loadSlaveConfig);