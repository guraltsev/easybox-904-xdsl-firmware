#!/bin/sh

### Following Assumptions are made in this script

#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++##
#	MODIY THE BELOW VARIABLES TO MATCH YOUR ISP SETTINGS 		##
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++##
#
# ETHERNET CONFIGURATIONS VALID FOR MII-1 ONLY!
MII_IF="eth1"
ETH_WANIDX="1"

# WANIP Connection Parameters
WAN_IP="200.206.32.120"
WAN_MASK="255.255.255.0"
WAN_GW="200.206.32.1"

# DNS Information
PRIM_DNS="125.22.47.125"
SEC_DNS="202.56.250.5"

# NAT Status 0 - Disable 1 -Enable (Default)
NAT_Status="1"
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++##

case "$1" in
	start)
		echo "Bringingup WAN StaticIP Connection on ($MII_IF:$ETH_WANIDX)"
		#Create the WAN (eth) interface
		/sbin/ifconfig $MII_IF 0.0.0.0 up
		/sbin/ifconfig $MII_IF:$ETH_WANIDX $WAN_IP netmask $WAN_MASK up > /dev/null 2> /dev/null

		# For NATed SScenerio enable Masquerading 
		if [ "$NAT_Status" = "1" ]; then
			/usr/sbin/iptables -t nat -A POSTROUTING -o $MII_IF -j MASQUERADE
		fi

		# Enable this if eth is the preferred default gw
		/sbin/route add default gw $WAN_GW

		# Stop the existing dnrd service and start again 
		# with configured DNS Servers
		/etc/rc.d/killproc dnrd > /dev/null 2> /dev/null
		sleep 3
		/usr/sbin/dnrd -s $PRIM_DNS -s $SEC_DNS	
		;;

	stop)
		echo "Stopping WAN StaticIP Connection on ($MII_IF:$ETH_WANIDX)"
		# Remove the default route thru' this eth interface
		/sbin/route del default gw $WAN_GW

		# Bring down this WAN interface
		/sbin/ifconfig $MII_IF:$ETH_WANIDX down > /dev/null 2> /dev/null

		# For NATed Scenerio remove Masquerading rule
		if [ "$NAT_Status" = "1" ]; then
			/usr/sbin/iptables -t nat -D POSTROUTING -o $MII_IF -j MASQUERADE
		fi

		# Stop the dnrd service
		/etc/rc.d/killproc dnrd > /dev/null 2> /dev/null
		;;

	*)
		echo "Usage $0 {start|stop}"
		exit 1
esac

