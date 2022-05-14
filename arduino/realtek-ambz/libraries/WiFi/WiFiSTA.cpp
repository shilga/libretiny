/* Copyright (c) Kuba Szczodrzyński 2022-04-25. */

#include "WiFi.h"
#include "WiFiPriv.h"

WiFiStatus WiFiClass::begin(char *ssid, char *passphrase, int32_t channel, const uint8_t *bssid, bool connect) {
	return begin((const char *)ssid, (const char *)passphrase, channel, bssid, connect);
}

WiFiStatus
WiFiClass::begin(const char *ssid, const char *passphrase, int32_t channel, const uint8_t *bssid, bool connect) {
	if (!enableSTA(true))
		return WL_CONNECT_FAILED;

	if (!ssid || *ssid == 0x00 || strlen(ssid) > 32) {
		LT_W("SSID not specified or too long");
		return WL_CONNECT_FAILED;
	}

	if (passphrase && strlen(passphrase) > 64) {
		LT_W("Passphrase too long");
		return WL_CONNECT_FAILED;
	}

	memset(wifi.bssid.octet, 0, ETH_ALEN);
	strcpy((char *)wifi.ssid.val, ssid);
	wifi.ssid.len = strlen(ssid);

	wifi.security_type = RTW_SECURITY_OPEN;
	wifi.password	   = NULL;
	wifi.password_len  = 0;
	wifi.key_id		   = 0;

	if (passphrase) {
		strcpy((char *)sta_password, passphrase);
		wifi.security_type = RTW_SECURITY_WPA2_AES_PSK;
		wifi.password	   = sta_password;
		wifi.password_len  = strlen(passphrase);
	}

	if (reconnect(bssid))
		return WL_CONNECTED;
	else
		return WL_CONNECT_FAILED;
}

bool WiFiClass::config(IPAddress localIP, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2) {
	if (!enableSTA(true))
		return false;
	struct netif *ifs = NETIF_RTW_STA;
	struct ip_addr ipaddr, netmask, gw, d1, d2;
	ipaddr.addr	 = localIP;
	netmask.addr = subnet;
	gw.addr		 = gateway;
	d1.addr		 = dns1;
	d2.addr		 = dns2;
	netif_set_addr(ifs, &ipaddr, &netmask, &gw);
	if (dns1[0])
		dns_setserver(0, &d1);
	if (dns2[0])
		dns_setserver(0, &d2);
	return true;
}

bool WiFiClass::reconnect(const uint8_t *bssid) {
	int ret;
	uint8_t dhcpRet;

	LT_I("Connecting to %s", wifi.ssid.val);

	if (!bssid) {
		ret = wifi_connect(
			(char *)wifi.ssid.val,
			wifi.security_type,
			(char *)wifi.password,
			wifi.ssid.len,
			wifi.password_len,
			wifi.key_id,
			NULL
		);
	} else {
		ret = wifi_connect_bssid(
			(unsigned char *)bssid,
			(char *)wifi.ssid.val,
			wifi.security_type,
			(char *)wifi.password,
			ETH_ALEN,
			wifi.ssid.len,
			wifi.password_len,
			wifi.key_id,
			NULL
		);
	}

	if (ret == RTW_SUCCESS) {
		dhcpRet = LwIP_DHCP(0, DHCP_START);
		if (dhcpRet == DHCP_ADDRESS_ASSIGNED)
			return true;
		LT_E("DHCP failed; dhcpRet=%d", dhcpRet);
		wifi_disconnect();
		return false;
	}
	LT_E("Connection failed; ret=%d", ret);
	return false;
}

bool WiFiClass::disconnect(bool wifiOff) {
	int ret = wifi_disconnect();
	if (wifiOff)
		enableSTA(false);
	return ret == RTW_SUCCESS;
}

bool WiFiClass::isConnected() {
	return status() == WL_CONNECTED;
}

bool WiFiClass::setAutoReconnect(bool autoReconnect) {
	return wifi_set_autoreconnect(autoReconnect) == RTW_SUCCESS;
}

bool WiFiClass::getAutoReconnect() {
	bool autoReconnect;
	wifi_get_autoreconnect((uint8_t *)&autoReconnect);
	return autoReconnect;
}

WiFiStatus WiFiClass::waitForConnectResult(unsigned long timeout) {
	if ((wifi_mode & WIFI_MODE_STA) == 0) {
		return WL_DISCONNECTED;
	}
	unsigned long start = millis();
	while ((!status() || status() >= WL_DISCONNECTED) && (millis() - start) < timeout) {
		delay(100);
	}
	return status();
}

IPAddress WiFiClass::localIP() {
	if (!wifi_mode)
		return IPAddress();
	return LwIP_GetIP(NETIF_RTW_STA);
}

uint8_t *WiFiClass::macAddress(uint8_t *mac) {
	uint8_t *macLocal = LwIP_GetMAC(NETIF_RTW_STA);
	memcpy(mac, macLocal, ETH_ALEN);
	free(macLocal);
	return mac;
}

String WiFiClass::macAddress(void) {
	uint8_t mac[ETH_ALEN];
	macAddress(mac);
	return macToString(mac);
}

IPAddress WiFiClass::subnetMask() {
	return LwIP_GetMASK(NETIF_RTW_STA);
}

IPAddress WiFiClass::gatewayIP() {
	return LwIP_GetGW(NETIF_RTW_STA);
}

IPAddress WiFiClass::dnsIP(uint8_t dns_no) {
	struct ip_addr dns;
	LwIP_GetDNS(&dns);
	return dns.addr;
}

IPAddress WiFiClass::broadcastIP() {
	return LwIP_GetBC(NETIF_RTW_STA);
}

IPAddress WiFiClass::networkID() {
	return calculateNetworkID(gatewayIP(), subnetMask());
}

uint8_t WiFiClass::subnetCIDR() {
	return calculateSubnetCIDR(subnetMask());
}

bool WiFiClass::enableIpV6() {
	return false;
}

IPv6Address WiFiClass::localIPv6() {
	return IPv6Address();
}

const char *WiFiClass::getHostname() {
	return netif_get_hostname(NETIF_RTW_STA);
}

bool WiFiClass::setHostname(const char *hostname) {
	netif_set_hostname(NETIF_RTW_STA, (char *)hostname);
	return true;
}

bool WiFiClass::setMacAddress(const uint8_t *mac) {
	return wifi_set_mac_address((char *)mac) == RTW_SUCCESS;
}

const String WiFiClass::SSID() {
	if (!isConnected())
		return "";
	wifi_get_setting(NETNAME_STA, &wifi_setting);
	return (char *)wifi_setting.ssid;
}

const String WiFiClass::psk() {
	if (!isConnected() || !wifi.password)
		return "";
	return (char *)wifi.password;
}

uint8_t *WiFiClass::BSSID() {
	uint8_t bssid[ETH_ALEN];
	wext_get_bssid(NETNAME_STA, bssid);
	return bssid;
}

String WiFiClass::BSSIDstr() {
	return macToString(BSSID());
}

int8_t WiFiClass::RSSI() {
	int rssi = 0;
	wifi_get_rssi(&rssi);
	return rssi;
}

WiFiAuthMode WiFiClass::getEncryption() {
	wifi_get_setting(NETNAME_STA, &wifi_setting);
	return WiFiClass::securityTypeToAuthMode(wifi_setting.security_type);
}