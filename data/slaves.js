let slaves = [];
let pollInterval = 10;
let timeout = 1;
let slaveStats = new Map();

function showStatus(message, type) {
    const status = document.getElementById('status');
    status.textContent = message;
    status.className = 'status-message ' + type;
    status.style.display = 'block';
    
    setTimeout(() => {
        status.style.display = 'none';
    }, 5000);
}

// Helper functions
function sortSlavesByID() {
    slaves.sort((a, b) => a.id - b.id);
}

function countUniqueSlaves() {
    const uniqueIDs = new Set();
    slaves.forEach(slave => uniqueIDs.add(slave.id));
    return uniqueIDs.size;
}

function addSlave() {
    const slaveId = document.getElementById('slave_id').value;
    const startReg = document.getElementById('start_reg').value;
    const numReg = document.getElementById('num_reg').value;
    const divider = document.getElementById('divider').value;
    const slaveName = document.getElementById('slave_name').value;
    const mqttTopic = document.getElementById('mqtt_topic').value;

    if (!slaveId || !startReg || !numReg || !divider || !slaveName || !mqttTopic) {
        showStatus('Please fill all fields', 'error');
        return;
    }

    // Check for duplicate ID + Name combination
    const duplicateExists = slaves.some(slave => 
        slave.id === parseInt(slaveId) && slave.name === slaveName
    );
    
    if (duplicateExists) {
        showStatus(`Error: Slave ID ${slaveId} already has name "${slaveName}"`, 'error');
        return;
    }

    const slave = {
        id: parseInt(slaveId),
        startReg: parseInt(startReg),
        numReg: parseInt(numReg),
        divider: parseFloat(divider),
        name: slaveName,
        mqttTopic: mqttTopic
    };

    slaves.push(slave);
    sortSlavesByID(); // Sort after adding
    updateSlavesList();
    clearSlaveForm();
    showStatus('Modbus slave added successfully!', 'success');
}

function clearSlaveForm() {
    document.getElementById('slave_id').value = '';
    document.getElementById('start_reg').value = '';
    document.getElementById('num_reg').value = '';
    document.getElementById('divider').value = '';
    document.getElementById('slave_name').value = '';
    document.getElementById('mqtt_topic').value = '';
}

function updateSlavesList() {
    const list = document.getElementById('slavesTableBody');
    const emptyState = document.getElementById('emptySlavesState');
    const slaveCount = document.getElementById('slaveCount');
    const slaveCountBadge = document.getElementById('slaveCountBadge');
    
    // Update statistics
    document.getElementById('totalEntries').textContent = slaves.length;
    const uniqueSlaveCount = countUniqueSlaves();
    slaveCount.textContent = uniqueSlaveCount;
    slaveCountBadge.textContent = slaves.length;
    
    if (slaves.length === 0) {
        list.innerHTML = '';
        emptyState.style.display = 'block';
        return;
    }
    
    emptyState.style.display = 'none';
    
    // Display the sorted slaves (array is already sorted)
    list.innerHTML = slaves.map((slave, index) => `
        <tr>
            <td><strong>${slave.id}</strong></td>
            <td>${slave.name}</td>
            <td>${slave.startReg}</td>
            <td>${slave.numReg}</td>
            <td>${slave.divider}</td>
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

// Slave Configuration Functions
async function saveSlaveConfig() {
    // slaves array is already sorted at this point
    const config = {
        slaves: slaves
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
            showStatus('Slave configuration saved successfully!', 'success');
        } else {
            showStatus('Error saving slave configuration', 'error');
        }
    } catch (error) {
        showStatus('Error saving slave configuration: ' + error, 'error');
    }
}

async function loadSlaveConfig() {
    try {
        const response = await fetch('/getslaves');
        if (response.ok) {
            const config = await response.json();
            slaves = config.slaves || [];
            sortSlavesByID(); // Sort after loading
            updateSlavesList();
            showStatus('Slave configuration loaded successfully!', 'success');
        } else {
            showStatus('Error loading slave configuration', 'error');
        }
    } catch (error) {
        showStatus('Error loading slave configuration: ' + error, 'error');
    }
}

// Poll Interval Functions
async function savePollInterval() {
    const interval = document.getElementById('poll_interval').value;
    if (!interval || interval < 1) {
        showStatus('Please enter a valid polling interval', 'error');
        return;
    }
    
    const config = {
        pollInterval: parseInt(interval)
    };

    try {
        const response = await fetch('/savepollinterval', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(config)
        });
        
        if (response.ok) {
            pollInterval = parseInt(interval);
            showStatus(`Poll interval saved: ${pollInterval} seconds`, 'success');
        } else {
            showStatus('Error saving poll interval', 'error');
        }
    } catch (error) {
        showStatus('Error saving poll interval: ' + error, 'error');
    }
}

async function loadPollInterval() {
    try {
        const response = await fetch('/getpollinterval');
        if (response.ok) {
            const config = await response.json();
            pollInterval = config.pollInterval || 10;
            
            document.getElementById('poll_interval').value = pollInterval;
            showStatus(`Poll interval loaded: ${pollInterval} seconds`, 'success');
        } else {
            showStatus('Error loading poll interval', 'error');
        }
    } catch (error) {
        showStatus('Error loading poll interval: ' + error, 'error');
    }
}

async function saveTimeout() {
    const timeoutValue = document.getElementById('timeout').value;
    if (!timeoutValue || timeoutValue < 1) {
        showStatus('Please enter a valid timeout (min 1 second)', 'error');
        return;
    }
    
    const config = {
        timeout: parseInt(timeoutValue)
    };

    try {
        const response = await fetch('/savetimeout', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(config)
        });
        
        if (response.ok) {
            timeout = parseInt(timeoutValue);
            showStatus(`Timeout saved: ${timeout} seconds`, 'success');
        } else {
            showStatus('Error saving timeout', 'error');
        }
    } catch (error) {
        showStatus('Error saving timeout: ' + error, 'error');
    }
}

async function loadTimeout() {
    try {
        const response = await fetch('/gettimeout');
        if (response.ok) {
            const config = await response.json();
            timeout = config.timeout || 1;
            
            document.getElementById('timeout').value = timeout;
            showStatus(`Timeout loaded: ${timeout} seconds`, 'success');
        } else {
            showStatus('Error loading timeout', 'error');
        }
    } catch (error) {
        showStatus('Error loading timeout: ' + error, 'error');
    }
}

function deleteSlaveStats(slaveId, slaveName) {
    const key = `${slaveId}-${slaveName}`;
    slaveStats.delete(key);
    updateStatsTable();
}

// Update existing functions to maintain statistics
function addSlave() {
    const slaveId = document.getElementById('slave_id').value;
    const startReg = document.getElementById('start_reg').value;
    const numReg = document.getElementById('num_reg').value;
    const divider = document.getElementById('divider').value;
    const slaveName = document.getElementById('slave_name').value;
    const mqttTopic = document.getElementById('mqtt_topic').value;

    if (!slaveId || !startReg || !numReg || !divider || !slaveName || !mqttTopic) {
        showStatus('Please fill all fields', 'error');
        return;
    }

    // Check for duplicate ID + Name combination
    const duplicateExists = slaves.some(slave => 
        slave.id === parseInt(slaveId) && slave.name === slaveName
    );
    
    if (duplicateExists) {
        showStatus(`Error: Slave ID ${slaveId} already has name "${slaveName}"`, 'error');
        return;
    }

    const slave = {
        id: parseInt(slaveId),
        startReg: parseInt(startReg),
        numReg: parseInt(numReg),
        divider: parseFloat(divider),
        name: slaveName,
        mqttTopic: mqttTopic
    };

    slaves.push(slave);
    sortSlavesByID();
    updateSlavesList();
    initializeSlaveStats(); // Initialize stats for new slave
    clearSlaveForm();
    showStatus('Modbus slave added successfully!', 'success');
}

function deleteSlave(index) {
    if (confirm('Are you sure you want to delete this Modbus slave?')) {
        const slave = slaves[index];
        slaves.splice(index, 1);
        deleteSlaveStats(slave.id, slave.name); // Remove stats for deleted slave
        updateSlavesList();
        showStatus('Modbus slave deleted successfully!', 'success');
    }
}

function updateSlavesList() {
    const list = document.getElementById('slavesTableBody');
    const emptyState = document.getElementById('emptySlavesState');
    const slaveCount = document.getElementById('slaveCount');
    const slaveCountBadge = document.getElementById('slaveCountBadge');
    
    // Update statistics
    document.getElementById('totalEntries').textContent = slaves.length;
    const uniqueSlaveCount = countUniqueSlaves();
    slaveCount.textContent = uniqueSlaveCount;
    slaveCountBadge.textContent = slaves.length;
    
    if (slaves.length === 0) {
        list.innerHTML = '';
        emptyState.style.display = 'block';
        // Also clear stats when no slaves
        slaveStats.clear();
        updateStatsTable();
        return;
    }
    
    emptyState.style.display = 'none';
    
    // Display the sorted slaves (array is already sorted)
    list.innerHTML = slaves.map((slave, index) => `
        <tr>
            <td><strong>${slave.id}</strong></td>
            <td>${slave.name}</td>
            <td>${slave.startReg}</td>
            <td>${slave.numReg}</td>
            <td>${slave.divider}</td>
            <td><code>${slave.mqttTopic}</code></td>
            <td>
                <button class="btn btn-small btn-warning" onclick="deleteSlave(${index})" title="Delete slave">
                    🗑️ Delete
                </button>
            </td>
        </tr>
    `).join('');
    
    // Initialize stats when slaves list updates
    initializeSlaveStats();
}


// Statistics functions
function initializeSlaveStats() {
    slaveStats.clear();
    // Initialize stats for existing slaves
    slaves.forEach(slave => {
        const key = `${slave.id}-${slave.name}`;
        if (!slaveStats.has(key)) {
            slaveStats.set(key, {
                slaveId: slave.id,
                slaveName: slave.name,
                totalQueries: 0,
                success: 0,
                timeout: 0,
                failed: 0
            });
        }
    });
    updateStatsTable();
}

function updateSlaveStats(slaveId, slaveName, type) {
    const key = `${slaveId}-${slaveName}`;
    
    if (!slaveStats.has(key)) {
        slaveStats.set(key, {
            slaveId: slave.id,
            slaveName: slave.name,
            totalQueries: 0,
            success: 0,
            timeout: 0,
            failed: 0
        });
    }
    
    const stats = slaveStats.get(key);
    stats.totalQueries++;
    
    switch(type) {
        case 'success':
            stats.success++;
            break;
        case 'timeout':
            stats.timeout++;
            break;
        case 'failed':
            stats.failed++;
            break;
    }
    
    updateStatsTable();
}

function updateStatsTable() {
    const tbody = document.getElementById('statsTableBody');
    const emptyState = document.getElementById('emptyStatsState');
    const statsCountBadge = document.getElementById('statsCountBadge');
    
    const statsArray = Array.from(slaveStats.values())
        .sort((a, b) => a.slaveId - b.slaveId);
    
    statsCountBadge.textContent = statsArray.length;
    
    if (statsArray.length === 0) {
        tbody.innerHTML = '';
        emptyState.style.display = 'block';
        return;
    }
    
    emptyState.style.display = 'none';
    
    tbody.innerHTML = statsArray.map(stats => `
        <tr>
            <td><strong>${stats.slaveId}</strong></td>
            <td>${stats.slaveName}</td>
            <td>${stats.totalQueries}</td>
            <td>${stats.success}</td>
            <td>${stats.timeout}</td>
            <td>${stats.failed}</td>
        </tr>
    `).join('');
}

// Load all configurations when page loads
document.addEventListener('DOMContentLoaded', function() {
    loadSlaveConfig();
    loadPollInterval();
    loadTimeout();
    // Initialize stats after slaves are loaded
    setTimeout(initializeSlaveStats, 1000);
});

