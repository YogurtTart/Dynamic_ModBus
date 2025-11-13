// ===== GLOBAL UTILITIES & SPA NAVIGATION =====

// ==================== STATUS MANAGEMENT ====================

class StatusManager {
    static showStatus(message, type = 'info') {
        const status = document.getElementById('status');
        if (!status) {
            console.warn('Status element not found');
            return;
        }
        
        // Clear existing timeout and animations
        if (window.statusTimeout) {
            clearTimeout(window.statusTimeout);
            window.statusTimeout = null;
        }
        
        status.classList.remove('hiding');
        status.textContent = message;
        status.className = `status-message ${type}`;
        status.style.display = 'block';
        
        // Auto-hide after 3 seconds
        window.statusTimeout = setTimeout(() => {
            status.classList.add('hiding');
            setTimeout(() => {
                status.style.display = 'none';
                status.classList.remove('hiding');
            }, 300);
        }, 3000);
    }
}

// ==================== API CLIENT ====================

class ApiClient {
// In utils.js - Update the ApiClient.request method
    static async request(url, options = {}) {
        try {
            const response = await fetch(url, options);
            
            if (!response.ok) {
                // Get the error message from response if available
                let errorMessage = `HTTP ${response.status}`;
                try {
                    const errorData = await response.json();
                    if (errorData.message) {
                        errorMessage = errorData.message;
                    } else if (errorData.error) {
                        errorMessage = errorData.error;
                    }
                } catch {
                    // If no JSON error message, use status-based messages
                    if (response.status === 400) {
                        errorMessage = 'Bad request - check your input data';
                    } else if (response.status === 404) {
                        errorMessage = 'Resource not found';
                    } else if (response.status === 500) {
                        errorMessage = 'Server error';
                    }
                }
                throw new Error(errorMessage);
            }
            
            return await response.json();
        } catch (error) {
            StatusManager.showStatus(`Error: ${error.message}`, 'error');
            throw error;
        }
    }

    static async get(url) {
        return this.request(url);
    }

    static async post(url, data) {
        return this.request(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
    }

    static async postForm(url, formData) {
        try {
            const response = await fetch(url, {
                method: 'POST',
                body: formData
            });
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return await response.text();
        } catch (error) {
            StatusManager.showStatus(`Error: ${error.message}`, 'error');
            throw error;
        }
    }
}

// ==================== FORM HELPER ====================

class FormHelper {
    static getElement(id) {
        return document.getElementById(id);
    }

    static getValue(id) {
        const element = this.getElement(id);
        return element ? element.value.trim() : '';
    }

    static setValue(id, value) {
        const element = this.getElement(id);
        if (element) element.value = value || '';
    }

    static clearForm(ids) {
        ids.forEach(id => this.setValue(id, ''));
    }

    static validateRequired(ids) {
        for (const id of ids) {
            const value = this.getValue(id);
            if (!value) {
                StatusManager.showStatus(`Please fill in ${id.replace(/_/g, ' ')}`, 'error');
                this.getElement(id)?.focus();
                return false;
            }
        }
        return true;
    }
}

// ==================== SPA NAVIGATION SYSTEM ====================

class SPANavigation {
    constructor() {
        this.currentTab = 'wifi';
        this.init();
    }

    init() {
        this.setupEventListeners();
        this.loadInitialTab();
    }

    setupEventListeners() {
        // Navigation click events
        document.querySelectorAll('.nav-item').forEach(item => {
            item.addEventListener('click', (e) => {
                e.preventDefault();
                const tab = item.getAttribute('data-tab');
                this.switchTab(tab);
            });
        });
    }

    loadInitialTab() {
        const hashTab = window.location.hash.replace('#', '');
        const activeNav = document.querySelector('.nav-item.active');
        
        if (hashTab && this.isValidTab(hashTab)) {
            this.switchTab(hashTab, false);
        } else if (activeNav) {
            const tab = activeNav.getAttribute('data-tab');
            this.switchTab(tab, false);
        } else {
            this.switchTab('wifi', false);
        }
    }

    isValidTab(tab) {
        return ['wifi', 'slaves', 'settings', 'debug'].includes(tab);
    }

    switchTab(tab, updateHistory = true) {
        if (!this.isValidTab(tab) || tab === this.currentTab) return;

        // Hide all tab contents
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.remove('active');
        });

        // Remove active class from all nav items
        document.querySelectorAll('.nav-item').forEach(item => {
            item.classList.remove('active');
        });

        // Show selected tab content
        const tabContent = document.getElementById(`${tab}-tab`);
        if (tabContent) {
            tabContent.classList.add('active');
        }

        // Activate corresponding nav item
        const navItem = document.querySelector(`.nav-item[data-tab="${tab}"]`);
        if (navItem) {
            navItem.classList.add('active');
        }

        // Update current tab
        this.currentTab = tab;

        // Trigger tab-specific initialization
        this.initializeTab(tab);

        // Dispatch custom event for other scripts
        window.dispatchEvent(new CustomEvent('tabChanged', { 
            detail: { newTab: tab }
        }));

        console.log(`Switched to tab: ${tab}`);
    }

    initializeTab(tab) {
        const tabManagers = {
            wifi: 'wifiManager',
            slaves: 'slavesManager', 
            settings: 'settingsManager',
            debug: 'debugConsole'
        };

        const manager = window[tabManagers[tab]];
        if (manager && typeof manager.refreshUI === 'function') {
            setTimeout(() => manager.refreshUI(), 100);
        }
    }

    getCurrentTab() {
        return this.currentTab;
    }

    goToTab(tab) {
        this.switchTab(tab);
    }

    refreshCurrentTab() {
        this.initializeTab(this.currentTab);
    }
}

// ==================== GLOBAL NAVIGATION FUNCTIONS ====================

function switchToTab(tabName) {
    if (window.spaNavigation) {
        window.spaNavigation.goToTab(tabName);
    }
}

function getCurrentTab() {
    return window.spaNavigation ? window.spaNavigation.getCurrentTab() : 'wifi';
}

function refreshCurrentTab() {
    if (window.spaNavigation) {
        window.spaNavigation.refreshCurrentTab();
    }
}

// ==================== GLOBAL INITIALIZATION ====================

document.addEventListener('DOMContentLoaded', function() {
    // Initialize SPA Navigation
    window.spaNavigation = new SPANavigation();
    
    console.log('ESP8266 SPA Navigation & Utilities initialized');
});

// ==================== ERROR HANDLING ====================

window.addEventListener('error', function(e) {
    console.error('SPA Navigation Error:', e.error);
    StatusManager.showStatus('An error occurred. Please refresh the page.', 'error');
});