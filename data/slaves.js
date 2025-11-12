// ==================== DEVICE TYPE CONSTANTS ====================

const DEVICE_TYPES = {
    G01S: "G01S",
    HeylaParam: "HeylaParam", 
    HeylaVoltage: "HeylaVoltage",
    HeylaEnergy: "HeylaEnergy"
};

// ==================== SLAVES MANAGER CLASS ====================

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
        const uniqueIDs = new Set(this.slaves.map(slave => slave.id));
        return uniqueIDs.size;
    }

    addSlave() {
        const slaveId = FormHelper.getValue('slave_id');
        const startReg = FormHelper.getValue('start_reg');
        const numReg = FormHelper.getValue('num_reg');
        const registerSize = FormHelper.getValue('register_size');
        const deviceType = FormHelper.getValue('device_type');
        const deviceIdentifier = document.getElementById('device_identifier').value.trim();
        const slaveName = FormHelper.getValue('slave_name');
        const mqttTopic = FormHelper.getValue('mqtt_topic');

        const requiredFields = ['slave_id', 'start_reg', 'num_reg', 'register_size', 'mqtt_topic'];
        if (!FormHelper.validateRequired(requiredFields)) return;

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

        const slave = {
            id: parseInt(slaveId),
            startReg: parseInt(startReg),
            numReg: parseInt(numReg),
            registerSize: parseInt(registerSize),
            name: slaveName,
            mqttTopic: mqttTopic,
            deviceType: deviceType
        };

        this.slaves.push(slave);
        this.sortSlavesByID();
        this.updateSlavesList();
        this.clearSlaveForm();
        StatusManager.showStatus('Modbus slave added successfully!', 'success');
    }

    clearSlaveForm() {
        FormHelper.clearForm(['slave_id', 'start_reg', 'num_reg', 'slave_name', 'mqtt_topic', 'device_identifier']);

        document.getElementById('register_size').value = 1;
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
        if (slaveCount) slaveCount.textContent = uniqueSlaveCount;
        if (slaveCountBadge) slaveCountBadge.textContent = this.slaves.length;
        
        if (this.slaves.length === 0) {
            if (list) list.innerHTML = '';
            if (emptyState) emptyState.style.display = 'block';
            return;
        }
        
        if (emptyState) emptyState.style.display = 'none';
        
        if (list) {
            list.innerHTML = this.slaves.map((slave, index) => `
                <tr>
                    <td><strong>${slave.id}</strong></td>
                    <td>${slave.name}</td>
                    <td>${slave.startReg}</td>
                    <td>${slave.numReg}</td>
                    <td>${slave.registerSize}</td>
                    <td><code>${slave.mqttTopic}</code></td>
                    <td>
                        <button class="btn btn-small btn-warning" onclick="slavesManager.deleteSlave(${index})" title="Delete slave">
                            🗑️ Delete
                        </button>
                    </td>
                </tr>
            `).join('');
        }
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
        const config = { 
            slaves: this.slaves.map(slave => ({
                id: slave.id,
                startReg: slave.startReg,
                numReg: slave.numReg,
                registerSize: slave.registerSize,
                name: slave.name,
                mqttTopic: slave.mqttTopic,
                deviceType: slave.deviceType
            }))
        };

        try {
            await ApiClient.post('/saveslaves', config);
            
            // Force reload slaves after save
            setTimeout(() => {
                this.loadSlaveConfig();
            }, 1000);
            
            StatusManager.showStatus('Slave configuration saved successfully!', 'success');
        } catch (error) {
            console.error("Error saving slaves:", error);
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
            if (emptyState) emptyState.style.display = 'none';
            if (statsCountBadge) statsCountBadge.textContent = filteredStats.length;
            
            this.smartRenderStats(tbody, filteredStats);
        } else {
            if (tbody) tbody.innerHTML = '';
            if (emptyState) emptyState.style.display = 'block';
            if (statsCountBadge) statsCountBadge.textContent = '0';
        }
    }

    smartRenderStats(tbody, filteredStats) {
        if (!tbody) return;
        
        const updatedRows = new Set();
        
        filteredStats.forEach(stats => {
            const rowId = `stats-row-${stats.slaveId}-${stats.slaveName.replace(/\s+/g, '-')}`;
            let row = document.getElementById(rowId);
            
            if (!row) {
                row = document.createElement('tr');
                row.id = rowId;
                tbody.appendChild(row);
            }
            
            this.updateStatsRow(row, stats);
            updatedRows.add(rowId);
        });
        
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

    refreshUI() {
        this.updateSlavesList();
        this.fetchStatistics();
        console.log('Slaves tab UI refreshed');
    }

    initCompositeNameInput() {
        const deviceType = document.getElementById('device_type');
        const deviceIdentifier = document.getElementById('device_identifier');
        const namePreview = document.getElementById('name_preview');
        const hiddenSlaveName = document.getElementById('slave_name');

        const updateName = () => {
            const type = deviceType.value;
            const identifier = deviceIdentifier.value.trim();
            const finalName = identifier ? `${type}_${identifier}` : type;
            
            if (hiddenSlaveName) hiddenSlaveName.value = finalName;
            if (namePreview) {
                namePreview.textContent = finalName;
                namePreview.style.color = identifier ? 'var(--text-primary)' : 'var(--error-color)';
            }
        };

        if (deviceType) deviceType.addEventListener('change', updateName);
        if (deviceIdentifier) deviceIdentifier.addEventListener('input', updateName);
        
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

// Make loadSlaveConfig globally accessible so other files can call it
window.saveSlaveConfig = () => {
    if (window.slavesManager) {
        return window.slavesManager.saveSlaveConfig();
    }
    console.warn('SlavesManager not initialized');
};