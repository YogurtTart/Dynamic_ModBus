// ==================== DEVICE TYPE CONSTANTS ====================

const DEVICE_TYPES = {
    G01S: "G01S",
    HeylaParam: "HeylaParam", 
    HeylaVoltage: "HeylaVoltage",
    HeylaEnergy: "HeylaEnergy"
};

// ==================== SLAVES MANAGER CLASS ====================

/**
 * @class SlavesManager - Manages Modbus slave configuration and statistics
 */
class SlavesManager {
    constructor() {
        this.slaves = [];
        this.pollInterval = 10;
        this.timeout = 1;
        this.statsPollInterval = null;
        this.init();
    }

    // ==================== INITIALIZATION ====================

    init() {
        this.bindEvents();
        this.initCompositeNameInput();
        this.loadAllConfigs();
        this.startStatsPolling();
    }

    bindEvents() {
        // Enter key support for slave form
        const slaveForm = document.getElementById('slaveForm');
        if (slaveForm) {
            slaveForm.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    this.addSlave();
                }
            });
        }
    }

    async loadAllConfigs() {
        try {
            await Promise.all([
                this.loadSlaveConfig(),
                this.loadPollingConfig(),
            ]);
        } catch (error) {
            console.error('Error loading configurations:', error);
        }
    }

    // ==================== SLAVE MANAGEMENT ====================

    sortSlavesByID() {
        this.slaves.sort((a, b) => a.id - b.id);
    }

    countUniqueSlaves() {
        const uniqueIDs = new Set();
        this.slaves.forEach(slave => uniqueIDs.add(slave.id));
        return uniqueIDs.size;
    }

    addSlave() {
        const slaveId = FormHelper.getValue('slave_id');
        const startReg = FormHelper.getValue('start_reg');
        const numReg = FormHelper.getValue('num_reg');
        const bitAddress = FormHelper.getValue('bit_address');
        const slaveName = FormHelper.getValue('slave_name');
        const mqttTopic = FormHelper.getValue('mqtt_topic');

        const requiredFields = ['slave_id', 'start_reg', 'num_reg', 'slave_name', 'bit_address', 'mqtt_topic'];
        if (!FormHelper.validateRequired(requiredFields)) return;

        // Validate that identifier is not empty
        const deviceIdentifier = document.getElementById('device_identifier').value.trim();
        if (!deviceIdentifier) {
            StatusManager.showStatus('Please enter a device identifier', 'error');
            return;
        }

        // Check for duplicate ID + Name combination
        const duplicateExists = this.slaves.some(slave => 
            slave.id === parseInt(slaveId) && slave.name === slaveName
        );
        
        if (duplicateExists) {
            StatusManager.showStatus(`Error: Slave ID ${slaveId} already has name "${slaveName}"`, 'error');
            return;
        }

        const slave = this.createSlaveConfig(slaveId, startReg, numReg, slaveName, bitAddress, mqttTopic);
        this.slaves.push(slave);
        this.sortSlavesByID();
        this.updateSlavesList();
        this.clearSlaveForm();
        StatusManager.showStatus('Modbus slave added successfully!', 'success');
    }

    createSlaveConfig(slaveId, startReg, numReg, slaveName, bitAddress, mqttTopic) {
        const slave = {
            id: parseInt(slaveId),
            startReg: parseInt(startReg),
            numReg: parseInt(numReg),
            bitAddress: parseInt(bitAddress),
            name: slaveName,
            mqttTopic: mqttTopic
        };

        // Add device-specific configurations
        if (slaveName.includes(DEVICE_TYPES.G01S)) {
            slave.tempdivider = 1;
            slave.humiddivider = 1;
        } else if (slaveName.includes(DEVICE_TYPES.HeylaParam)) {
            this.addMeterConfig(slave);
        } else if (slaveName.includes(DEVICE_TYPES.HeylaVoltage)) {
            this.addVoltageConfig(slave);
        } else if (slaveName.includes(DEVICE_TYPES.HeylaEnergy)) {
            this.addEnergyConfig(slave);
        }

        return slave;
    }

    addMeterConfig(slave) {
        const meterConfigs = [
            'ACurrent', 'BCurrent', 'CCurrent', 'ZeroPhaseCurrent',
            'AActiveP', 'BActiveP', 'CActiveP', 'Total3PActiveP',
            'AReactiveP', 'BReactiveP', 'CReactiveP', 'Total3PReactiveP',
            'AApparentP', 'BApparentP', 'CApparentP', 'Total3PApparentP',
            'APowerF', 'BPowerF', 'CPowerF', 'Total3PPowerF'
        ];
        
        meterConfigs.forEach(config => {
            slave[config] = { ct: 1, pt: 1, divider: 1 };
        });
    }

    addVoltageConfig(slave) {
        const voltageConfigs = [
            'AVoltage', 'BVoltage', 'CVoltage', 
            'PhaseVoltageMean', 'ZeroSequenceVoltage'
        ];
        
        voltageConfigs.forEach(config => {
            slave[config] = { pt: 1.0, divider: 1.0 };
        });
    }

    addEnergyConfig(slave) {
        slave.totalActiveEnergy = { divider: 1.0 };
        slave.importActiveEnergy = { divider: 1.0 };
        slave.exportActiveEnergy = { divider: 1.0 };
    }

    clearSlaveForm() {
        FormHelper.clearForm(['slave_id', 'start_reg', 'num_reg', 'slave_name', 'mqtt_topic', 'bit_address', 'device_identifier']);

        document.getElementById('device_type').value = DEVICE_TYPES.G01S;
        document.getElementById('name_preview').textContent = DEVICE_TYPES.G01S;
        document.getElementById('name_preview').style.color = 'var(--error-color)';
    }

    // ==================== UI UPDATES ====================

    updateSlavesList() {
        const list = document.getElementById('slavesTableBody');
        const emptyState = document.getElementById('emptySlavesState');
        const slaveCount = document.getElementById('slaveCount');
        const slaveCountBadge = document.getElementById('slaveCountBadge');
        
        // Update statistics
        document.getElementById('totalEntries').textContent = this.slaves.length;
        const uniqueSlaveCount = this.countUniqueSlaves();
        slaveCount.textContent = uniqueSlaveCount;
        slaveCountBadge.textContent = this.slaves.length;
        
        if (this.slaves.length === 0) {
            list.innerHTML = '';
            emptyState.style.display = 'block';
            return;
        }
        
        emptyState.style.display = 'none';
        
        list.innerHTML = this.slaves.map((slave, index) => `
            <tr>
                <td><strong>${slave.id}</strong></td>
                <td>${slave.name}</td>
                <td>${slave.startReg}</td>
                <td>${slave.numReg}</td>
                <td>${slave.bitAddress}-bit</td>
                <td><code>${slave.mqttTopic}</code></td>
                <td>
                    <button class="btn btn-small btn-warning" onclick="slavesManager.deleteSlave(${index})" title="Delete slave">
                        🗑️ Delete
                    </button>
                </td>
            </tr>
        `).join('');
    }

    deleteSlave(index) {
        if (confirm('Are you sure you want to delete this Modbus slave?')) {
            const slave = this.slaves[index];
            this.slaves.splice(index, 1);
            this.updateSlavesList();
            this.removeSlaveStats(slave.id, slave.name);
            StatusManager.showStatus('Modbus slave deleted successfully!', 'success');
        }
    }

    // ==================== CONFIGURATION PERSISTENCE ====================

    async saveSlaveConfig() {
        const config = { slaves: this.slaves };

        try {
            await ApiClient.post('/saveslaves', config);
            StatusManager.showStatus('Slave configuration saved successfully!', 'success');
        } catch (error) {
            // Error handled by ApiClient
        }
    }

    async loadSlaveConfig() {
        try {
            const config = await ApiClient.get('/getslaves');
            this.slaves = config.slaves || [];
            this.sortSlavesByID();
            this.updateSlavesList();
            StatusManager.showStatus('Slave configuration loaded successfully!', 'success');
        } catch (error) {
            // Error handled by ApiClient
        }
    }

    async savePollingConfig() {
        const interval = FormHelper.getValue('poll_interval');
        const timeoutValue = FormHelper.getValue('timeout');
        
        if (!interval || interval < 1 || !timeoutValue || timeoutValue < 1) {
            StatusManager.showStatus('Please enter valid interval and timeout values', 'error');
            return;
        }

        try {
            await ApiClient.post('/savepollingconfig', { 
                pollInterval: parseInt(interval), 
                timeout: parseInt(timeoutValue) 
            });
            this.pollInterval = parseInt(interval);
            this.timeout = parseInt(timeoutValue);
            StatusManager.showStatus(`Polling config saved: ${this.pollInterval}s interval, ${this.timeout}s timeout`, 'success');
        } catch (error) {
            // Error handled by ApiClient
        }
    }

    async loadPollingConfig() {
        try {
            const config = await ApiClient.get('/getpollingconfig');
            this.pollInterval = config.pollInterval || 10;
            this.timeout = config.timeout || 1;
            
            FormHelper.setValue('poll_interval', this.pollInterval);
            FormHelper.setValue('timeout', this.timeout);
            
            StatusManager.showStatus(`Polling config loaded: ${this.pollInterval}s interval, ${this.timeout}s timeout`, 'success');
        } catch (error) {
            // Error handled by ApiClient
        }
    }

    // ==================== STATISTICS MANAGEMENT ====================

    startStatsPolling() {
        this.statsPollInterval = setInterval(() => this.fetchStatistics(), 2000);
    }

    async fetchStatistics() {
        try {
            const statsArray = await ApiClient.get('/getstatistics');
            this.updateStatsDisplay(statsArray);
        } catch (error) {
            console.log('Error fetching statistics:', error);
        }
    }

    updateStatsDisplay(statsArray) {
        const tbody = document.getElementById('statsTableBody');
        const emptyState = document.getElementById('emptyStatsState');
        const statsCountBadge = document.getElementById('statsCountBadge');
        
        const filteredStats = (statsArray || [])
            .filter(stats => this.slaves.some(slave => 
                slave.id === stats.slaveId && slave.name === stats.slaveName
            ))
            .sort((a, b) => a.slaveId - b.slaveId);
        
        if (filteredStats.length > 0) {
            emptyState.style.display = 'none';
            statsCountBadge.textContent = filteredStats.length;
            
            // Use smart rendering instead of innerHTML
            this.smartRenderStats(tbody, filteredStats);
        } else {
            tbody.innerHTML = '';
            emptyState.style.display = 'block';
            statsCountBadge.textContent = '0';
        }
    }

    smartRenderStats(tbody, filteredStats) {
        // Track which rows we've updated
        const updatedRows = new Set();
        
        filteredStats.forEach(stats => {
            const rowId = `stats-row-${stats.slaveId}-${stats.slaveName.replace(/\s+/g, '-')}`;
            let row = document.getElementById(rowId);
            
            if (!row) {
                // Create new row
                row = document.createElement('tr');
                row.id = rowId;
                tbody.appendChild(row);
            }
            
            // Update row content
            this.updateStatsRow(row, stats);
            updatedRows.add(rowId);
        });
        
        // Remove rows that are no longer in the data
        this.removeOldStatsRows(tbody, updatedRows);
    }

    updateStatsRow(row, stats) {
        const rowContent = `
            <td><strong>${stats.slaveId}</strong></td>
            <td>${stats.slaveName}</td>
            <td>${stats.totalQueries}</td>
            <td>${stats.success}</td>
            <td>${stats.timeout}</td>
            <td>${stats.failed}</td>
            <td>
                <div class="status-history">
                    ${this.renderStatusHistory(stats.statusHistory)}
                </div>
            </td>
        `;
        
        if (row.innerHTML !== rowContent) {
            row.innerHTML = rowContent;
        }
    }

    removeOldStatsRows(tbody, updatedRows) {
        const allRows = tbody.querySelectorAll('tr');
        allRows.forEach(row => {
            if (!updatedRows.has(row.id)) {
                row.remove();
            }
        });
    }

    /**
     * @brief Render status history as colored circles
     */
    renderStatusHistory(statusHistory) {
        if (!statusHistory || statusHistory.length < 3) {
            return '<div class="status-history">---</div>';
        }
        
        const statusMap = {
            'S': 'status-success',
            'F': 'status-failure', 
            'T': 'status-timeout'
        };
        
        let circles = '';
        for (let i = 0; i < 3; i++) {
            const statusClass = statusMap[statusHistory[i]];
            
            // ALWAYS pulse the newest (leftmost) circle, others never pulse
            const pulseClass = (i === 0) ? 'status-pulse' : '';
            
            circles += `<span class="status-circle ${statusClass} ${pulseClass}"></span>`;
        }
        
        return circles;
    }

    async removeSlaveStats(slaveId, slaveName) {
        try {
            await ApiClient.post('/removeslavestats', {
                slaveId: slaveId,
                slaveName: slaveName
            });
            console.log(`Removed statistics for slave ${slaveId}: ${slaveName}`);
            this.fetchStatistics();
        } catch (error) {
            console.log('Error removing slave statistics:', error);
        }
    }

    // ==================== UI MANAGEMENT ====================

    /**
     * @brief Refresh UI when slaves tab becomes active
     */
    refreshUI() {
        this.updateSlavesList();
        this.updateStatsDisplay();
        console.log('Slaves tab UI refreshed');
    }

    /**
     * @brief Initialize composite name input with device type and identifier
     */
    initCompositeNameInput() {
        const deviceType = document.getElementById('device_type');
        const deviceIdentifier = document.getElementById('device_identifier');
        const namePreview = document.getElementById('name_preview');
        const hiddenSlaveName = document.getElementById('slave_name');

        const updateName = () => {
            const type = deviceType.value;
            const identifier = deviceIdentifier.value.trim();
            const finalName = identifier ? `${type}_${identifier}` : type;
            
            hiddenSlaveName.value = finalName;
            namePreview.textContent = finalName;
            
            // Visual feedback
            if (identifier) {
                namePreview.style.color = 'var(--text-primary)';
            } else {
                namePreview.style.color = 'var(--error-color)';
            }
        };

        deviceType.addEventListener('change', updateName);
        deviceIdentifier.addEventListener('input', updateName);
        
        // Initialize
        updateName();
    }
}

// ==================== GLOBAL INITIALIZATION ====================

// Initialize immediately for SPA
window.slavesManager = new SlavesManager();

// Refresh UI when slaves tab becomes active
window.addEventListener('tabChanged', (e) => {
    if (e.detail.newTab === 'slaves') {
        window.slavesManager.refreshUI();
    }
});