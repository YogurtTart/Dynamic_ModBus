let slaves = [];
let pollInterval = 10;
let timeout = 1;
let slaveStats = new Map();

function showStatus(message, type) {
    const status = document.getElementById('status');
    
    // Remove any existing hiding class
    status.classList.remove('hiding');
    
    // Set message content and styling
    status.textContent = message;
    status.className = 'status-message ' + type;
    
    // Show the message with animation
    status.style.display = 'block';
    
    // Auto-hide after 5 seconds with animation
    setTimeout(() => {
        status.classList.add('hiding');
        setTimeout(() => {
            status.style.display = 'none';
            status.classList.remove('hiding');
        }, 300);
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
    const slaveName = document.getElementById('slave_name').value;
    const mqttTopic = document.getElementById('mqtt_topic').value;

    if (!slaveId || !startReg || !numReg || !slaveName || !mqttTopic) {
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
        name: slaveName,
        mqttTopic: mqttTopic
    };

    // Add additional values based on device name
    if (slaveName.includes('Sensor')) {
        slave.tempdivider = 1; // Default divider value
        slave.humiddivider = 1; // Default divider value
    } else if (slaveName.includes('Meter')) {
        // Add 19 meter-specific values
        slave.ACurrent = { ct: 1, pt: 1, divider: 1 };
        slave.BCurrent = { ct: 1, pt: 1, divider: 1 };
        slave.CCurrent = { ct: 1, pt: 1, divider: 1 };
        slave.ZeroPhaseCurrent = { ct: 1, pt: 1, divider: 1 };
        slave.AActiveP = { ct: 1, pt: 1, divider: 1 };
        slave.BActiveP = { ct: 1, pt: 1, divider: 1 };
        slave.CActiveP = { ct: 1, pt: 1, divider: 1 };
        slave.Total3PActiveP = { ct: 1, pt: 1, divider: 1 };
        slave.AReactiveP = { ct: 1, pt: 1, divider: 1 };
        slave.BReactiveP = { ct: 1, pt: 1, divider: 1 };
        slave.CReactiveP = { ct: 1, pt: 1, divider: 1 };
        slave.Total3PReactiveP = { ct: 1, pt: 1, divider: 1 };
        slave.AApparentP = { ct: 1, pt: 1, divider: 1 };
        slave.BApparentP = { ct: 1, pt: 1, divider: 1 };
        slave.CApparentP = { ct: 1, pt: 1, divider: 1 };
        slave.Total3PApparentP = { ct: 1, pt: 1, divider: 1 };
        slave.APowerF = { ct: 1, pt: 1, divider: 1 };
        slave.BPowerF = { ct: 1, pt: 1, divider: 1 };
        slave.CPowerF = { ct: 1, pt: 1, divider: 1 };
        slave.Total3PPowerF = { ct: 1, pt: 1, divider: 1 };
    } else if (slaveName.includes('Voltage')) {
        // ✅ NEW: Voltage device - separate from Meter
        slave.AVoltage = { pt: 1.0, divider: 1.0 };
        slave.BVoltage = { pt: 1.0, divider: 1.0 };
        slave.CVoltage = { pt: 1.0, divider: 1.0 };
        slave.PhaseVoltageMean = { pt: 1.0, divider: 1.0 };
        slave.ZeroSequenceVoltage = { pt: 1.0, divider: 1.0 };
    } else if (slaveName.includes('Energy')) {
    // Single-phase energy meter with 32-bit energy values
    slave.totalActiveEnergy = { divider: 1.0 };
    slave.importActiveEnergy = { divider: 1.0 };
    slave.exportActiveEnergy = { divider: 1.0 };
    }

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
        const slave = slaves[index];
        slaves.splice(index, 1);
        updateSlavesList();
        // Remove statistics from backend
        removeSlaveStats(slave.id, slave.name);
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

// Statistics functions - REAL-TIME FROM BACKEND
let statsPollInterval = null;

function startStatsPolling() {
    // Poll every 5 seconds for real-time updates from backend
    statsPollInterval = setInterval(fetchStatistics, 5000);
}

async function fetchStatistics() {
    try {
        const response = await fetch('/getstatistics');
        if (response.ok) {
            const statsArray = await response.json();
            updateStatsDisplay(statsArray);
        }
    } catch (error) {
        console.log('Error fetching statistics:', error);
    }
}

function updateStatsDisplay(statsArray) {
    const tbody = document.getElementById('statsTableBody');
    const emptyState = document.getElementById('emptyStatsState');
    const statsCountBadge = document.getElementById('statsCountBadge');
    
    if (statsArray && statsArray.length > 0) {
        emptyState.style.display = 'none';
        statsCountBadge.textContent = statsArray.length;
        
        // Sort by slave ID and update table with backend data
        const sortedStats = [...statsArray].sort((a, b) => a.slaveId - b.slaveId);
        
        tbody.innerHTML = sortedStats.map(stats => `
            <tr>
                <td><strong>${stats.slaveId}</strong></td>
                <td>${stats.slaveName}</td>
                <td>${stats.totalQueries}</td>
                <td>${stats.success}</td>
                <td>${stats.timeout}</td>
                <td>${stats.failed}</td>
            </tr>
        `).join('');
    } else {
        tbody.innerHTML = '';
        emptyState.style.display = 'block';
        statsCountBadge.textContent = '0';
    }
}

function initializeSlaveStats() {
    // Just fetch initial stats from backend
    fetchStatistics();
}

// Load all configurations when page loads
document.addEventListener('DOMContentLoaded', function() {
    loadSlaveConfig();
    loadPollInterval();
    loadTimeout();
    // Start real-time statistics polling from backend
    startStatsPolling();
});

async function removeSlaveStats(slaveId, slaveName) {
    const config = {
        slaveId: slaveId,
        slaveName: slaveName
    };

    try {
        const response = await fetch('/removeslavestats', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(config)
        });
        
        if (response.ok) {
            console.log(`Removed statistics for slave ${slaveId}: ${slaveName}`);
        } else {
            console.log('Error removing slave statistics');
        }
    } catch (error) {
        console.log('Error removing slave statistics: ' + error);
    }
}