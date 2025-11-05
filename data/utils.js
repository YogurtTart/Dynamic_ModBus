// ===== GLOBAL UTILITIES & SPA NAVIGATION =====

// ==================== STATUS MANAGEMENT ====================

/**
 * @class StatusManager - Manages status message display
 */
class StatusManager {
    /**
     * @brief Show status message with specified type
     */
    static showStatus(message, type = 'info') {
        const status = document.getElementById('status');
        if (!status) {
            console.warn('Status element not found');
            return;
        }
        
        status.classList.remove('hiding');
        status.textContent = message;
        status.className = 'status-message ' + type;
        status.style.display = 'block';
        
        setTimeout(() => {
            status.classList.add('hiding');
            setTimeout(() => {
                status.style.display = 'none';
                status.classList.remove('hiding');
            }, 300);
        }, 5000);
    }
}

// ==================== API CLIENT ====================

/**
 * @class ApiClient - Handles HTTP requests to the ESP8266 server
 */
class ApiClient {
    /**
     * @brief Make HTTP request with error handling
     */
    static async request(url, options = {}) {
        try {
            const response = await fetch(url, options);
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return await response.json();
        } catch (error) {
            StatusManager.showStatus(`Error: ${error.message}`, 'error');
            throw error;
        }
    }

    /**
     * @brief Make GET request
     */
    static async get(url) {
        return this.request(url);
    }

    /**
     * @brief Make POST request with JSON data
     */
    static async post(url, data) {
        return this.request(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
    }

    /**
     * @brief Make POST request with FormData
     */
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

/**
 * @class FormHelper - Utility functions for form management
 */
class FormHelper {

    static getElement(id) {
        return document.getElementById(id);
    }

    /**
     * @brief Get value from form element
     */
    static getValue(id) {
        const element = this.getElement(id);
        return element ? element.value : '';
    }

    /**
     * @brief Set value of form element
     */
    static setValue(id, value) {
        const element = this.getElement(id);
        if (element) element.value = value || '';
    }

    /**
     * @brief Clear multiple form fields
     */
    static clearForm(ids) {
        ids.forEach(id => this.setValue(id, ''));
    }

    /**
     * @brief Validate required form fields
     */
    static validateRequired(ids) {
        for (const id of ids) {
            if (!this.getValue(id)) {
                StatusManager.showStatus(`Please fill in ${id}`, 'error');
                this.getElement(id)?.focus();
                return false;
            }
        }
        return true;
    }
}

// ==================== SPA NAVIGATION SYSTEM ====================

/**
 * @class SPANavigation - Single Page Application navigation controller
 */
class SPANavigation {
    constructor() {
        this.currentTab = 'wifi';
        this.init();
    }

    /**
     * @brief Initialize navigation system
     */
    init() {
        this.setupEventListeners();
        this.loadInitialTab();
    }

    /**
     * @brief Set up navigation event listeners
     */
    setupEventListeners() {
        // Navigation click events
        document.querySelectorAll('.nav-item').forEach(item => {
            item.addEventListener('click', (e) => {
                const tab = item.getAttribute('data-tab');
                this.switchTab(tab);
            });
        });
    }

    /**
     * @brief Load initial tab based on URL hash or active nav
     */
    loadInitialTab() {
        // Check URL hash first, then fallback to active nav item
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

    /**
     * @brief Check if tab name is valid
     */
    isValidTab(tab) {
        return ['wifi', 'slaves', 'settings', 'debug'].includes(tab);
    }

    /**
     * @brief Switch to specified tab
     */
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

    /**
     * @brief Initialize tab-specific functionality
     */
    initializeTab(tab) {
        switch(tab) {
            case 'wifi':
                this.initializeWifiTab();
                break;
            case 'slaves':
                this.initializeSlavesTab();
                break;
            case 'settings':
                this.initializeSettingsTab();
                break;
            case 'debug':
                this.initializeDebugTab();
                break;
        }
    }

    /**
     * @brief Initialize WiFi tab
     */
    initializeWifiTab() {
        // Refresh WiFi UI when tab becomes active
        if (window.wifiManager && typeof window.wifiManager.refreshUI === 'function') {
            setTimeout(() => window.wifiManager.refreshUI(), 100);
        }
    }

    /**
     * @brief Initialize slaves tab
     */
    initializeSlavesTab() {
        // Refresh slaves UI when tab becomes active
        if (window.slavesManager && typeof window.slavesManager.refreshUI === 'function') {
            setTimeout(() => window.slavesManager.refreshUI(), 100);
        }
    }

    /**
     * @brief Initialize settings tab
     */
    initializeSettingsTab() {
        // Refresh settings UI when tab becomes active
        if (window.settingsManager && typeof window.settingsManager.refreshUI === 'function') {
            setTimeout(() => window.settingsManager.refreshUI(), 100);
        }
    }

    /**
     * @brief Initialize debug tab
     */
    initializeDebugTab() {
        // Refresh debug UI when tab becomes active
        if (window.debugConsole && typeof window.debugConsole.refreshUI === 'function') {
            setTimeout(() => window.debugConsole.refreshUI(), 100);
        }
    }

    /**
     * @brief Get current active tab
     */
    getCurrentTab() {
        return this.currentTab;
    }

    /**
     * @brief Programmatically switch tabs
     */
    goToTab(tab) {
        this.switchTab(tab);
    }

    /**
     * @brief Refresh current tab
     */
    refreshCurrentTab() {
        this.initializeTab(this.currentTab);
    }
}

// ==================== GLOBAL NAVIGATION FUNCTIONS ====================

/**
 * @brief Switch to specified tab (global function)
 */
function switchToTab(tabName) {
    if (window.spaNavigation) {
        window.spaNavigation.goToTab(tabName);
    }
}

/**
 * @brief Get current active tab (global function)
 */
function getCurrentTab() {
    return window.spaNavigation ? window.spaNavigation.getCurrentTab() : 'wifi';
}

/**
 * @brief Refresh current tab (global function)
 */
function refreshCurrentTab() {
    if (window.spaNavigation) {
        window.spaNavigation.refreshCurrentTab();
    }
}

// ==================== GLOBAL INITIALIZATION ====================

/**
 * @brief Initialize when DOM is loaded
 */
document.addEventListener('DOMContentLoaded', function() {
    // Initialize SPA Navigation
    window.spaNavigation = new SPANavigation();
    
    // Add loading state management
    let isLoading = false;
    
    /**
     * @brief Enhanced status message function that works across tabs
     */
    window.showStatusMessage = function(message, type = 'info', duration = 5000) {
        const statusElement = document.getElementById('status');
        if (!statusElement) return;
        
        // Clear any existing timeout
        if (window.statusTimeout) {
            clearTimeout(window.statusTimeout);
            window.statusTimeout = null;
        }
        
        // Remove existing classes
        statusElement.className = 'status-message';
        
        // Set new message and type
        statusElement.textContent = message;
        statusElement.classList.add(type);
        statusElement.style.display = 'block';
        
        // Auto-hide after duration
        window.statusTimeout = setTimeout(() => {
            statusElement.classList.add('hiding');
            setTimeout(() => {
                statusElement.style.display = 'none';
                statusElement.classList.remove('hiding', type);
            }, 300);
        }, duration);
    };
    
    /**
     * @brief Enhanced loading state function
     */
    window.setLoadingState = function(button, isLoading) {
        if (!button) return;
        
        if (isLoading) {
            button.disabled = true;
            button.classList.add('loading');
        } else {
            button.disabled = false;
            button.classList.remove('loading');
        }
    };
    
    console.log('ESP8266 SPA Navigation & Utilities initialized');
});

// ==================== ERROR HANDLING ====================

window.addEventListener('error', function(e) {
    console.error('SPA Navigation Error:', e.error);
    if (window.showStatusMessage) {
        window.showStatusMessage('An error occurred. Please refresh the page.', 'error');
    }
});