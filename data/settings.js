// settings.js - Slave configuration editor
class SettingsManager {
    constructor() {
        this.currentSlaveId = null;
        this.currentSlaveName = null;
        this.init();
    }

    init() {
        this.bindEvents();
    }

    bindEvents() {
        // Enter key support for search form
        const searchForm = document.getElementById('searchForm');
        if (searchForm) {
            searchForm.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    this.searchSlaveConfig();
                }
            });
        }
    }

    async searchSlaveConfig() {
        const slaveId = FormHelper.getValue('search_slave_id');
        const slaveName = FormHelper.getValue('search_slave_name');

        if (!FormHelper.validateRequired(['search_slave_id', 'search_slave_name'])) return;

        try {
            StatusManager.showStatus('Searching for slave configuration...', 'info');
            
            const slaveConfig = await ApiClient.post('/getslaveconfig', {
                slaveId: parseInt(slaveId),
                slaveName: slaveName
            });

            this.currentSlaveId = parseInt(slaveId);
            this.currentSlaveName = slaveName;

            const editor = document.getElementById('config_editor');
            if (editor) {
                editor.value = JSON.stringify(slaveConfig, null, 2);
            }
            
            const editSection = document.getElementById('editSection');
            if (editSection) {
                editSection.style.display = 'block';
            }
            
            StatusManager.showStatus(`Found slave configuration for ${slaveName} (ID: ${slaveId})`, 'success');
            
        } catch (error) {
            StatusManager.showStatus('Error searching for slave: ' + error.message, 'error');
            console.error('Search error:', error);
        }
    }

    async saveEditedConfig() {
        if (!this.currentSlaveId || !this.currentSlaveName) {
            StatusManager.showStatus('No slave configuration loaded to save', 'error');
            return;
        }

        const editor = document.getElementById('config_editor');
        if (!editor) {
            StatusManager.showStatus('Configuration editor not found', 'error');
            return;
        }

        const editedConfig = editor.value.trim();
        if (!editedConfig) {
            StatusManager.showStatus('Configuration is empty', 'error');
            return;
        }

        try {
            const parsedConfig = JSON.parse(editedConfig);
            
            if (!parsedConfig.id || !parsedConfig.name) {
                StatusManager.showStatus('Configuration must include id and name fields', 'error');
                return;
            }

            if (parsedConfig.id !== this.currentSlaveId || parsedConfig.name !== this.currentSlaveName) {
                StatusManager.showStatus('Cannot change Slave ID or Name during edit', 'error');
                return;
            }

            const result = await ApiClient.post('/updateslaveconfig', parsedConfig);
            
            StatusManager.showStatus(result.message || 'Slave configuration updated successfully!', 'success');
            
            // 🛑 FIX: Don't call cancelEdit() here - just clear the form and hide the editor
            FormHelper.clearForm(['search_slave_id', 'search_slave_name']);
            
            const editSection = document.getElementById('editSection');
            if (editSection) editSection.style.display = 'none';
            if (editor) editor.value = '';
            
            this.currentSlaveId = null;
            this.currentSlaveName = null;

            setTimeout(() => {
                if (typeof saveSlaveConfig === 'function') {
                    saveSlaveConfig();
                }
            }, 1000);
            
        } catch (error) {
            if (error instanceof SyntaxError) {
                StatusManager.showStatus('Invalid JSON format. Please check your configuration.', 'error');
            } else {
                StatusManager.showStatus('Error saving configuration: ' + error.message, 'error');
            }
            console.error('Save error:', error);
        }
    }

    cancelEdit() {
        const editSection = document.getElementById('editSection');
        const editor = document.getElementById('config_editor');
        
        if (editSection) editSection.style.display = 'none';
        if (editor) editor.value = '';
        
        this.currentSlaveId = null;
        this.currentSlaveName = null;
        StatusManager.showStatus('Edit cancelled', 'info');
    }

    refreshUI() {
        // Reset forms when settings tab becomes active
        FormHelper.clearForm(['search_slave_id', 'search_slave_name']);
        
        const editSection = document.getElementById('editSection');
        if (editSection) editSection.style.display = 'none';
        
        this.currentSlaveId = null;
        this.currentSlaveName = null;
        
        console.log('Settings tab UI refreshed');
    }
}

// Initialize immediately for SPA
window.settingsManager = new SettingsManager();

// Refresh UI when settings tab becomes active
window.addEventListener('tabChanged', (e) => {
    if (e.detail.newTab === 'settings') {
        window.settingsManager.refreshUI();
    }
});