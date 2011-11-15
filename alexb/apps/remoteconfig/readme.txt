This application can be used to manage remotely the lifecycle of VMs on a machine running the
NOVA microhypervisor, the NOVA Virtual Machine Monitor Vancouver and the NOVA
user land (NUL).

Via libvirt[0] (with NOVA support enabled [1]) you can connect to this application
and start/stop Virtual Machines.

In order to use SSL/TLS the NOVA daemon requires 3 files, one containg the private
key, the other one a certificate about the public key of the daemon and the
third one a certificate of the CA which signed the daemon's certificate.
The certificate and the private key must be in DER(ASN.1) format.

Via the command line of the daemon you have to specify the files of the
certificates and the private key
('serverkey=<file>', 'servercert=<file>', 'cacert=<file>').

Additionally you can specify template VM configurations which are predeployed and may
be started instantiated multiple times. The syntax is as following:
 <name>:<file>
<name> means here the name to be shown via libvirt and <file> references a
file loaded via some fileservice provider (like fs/rom).

An example configuration for the daemon looks like that - daemon.nulconfig:

  sigma0::mem:12 name::/s0/timer name::/s0/log name::/s0/config name::/s0/fs/rom \
    name::/s0/admission namespace::/daemon name::/s0/events quota::guid ||
  rom://bin/apps/remote_config.nul.gz \
    fiascoVM:rom://fiasco/fiasco.nulconfig \
    lwipTest:rom://cfg/lwip.nulconfig \
    linuxVM:rom://cfg/linux.nulconfig \
    serverkey=rom://cfg/daemon_servkey.pem \
    servercert=rom://cfg/daemon_server.crt \
    cacert=rom://cfg/daemon_ca.crt 

[0] www.libvirt.org
[1] libvirt patch for NOVA+NUL, see */apps/libvirt 


=============================================================================
Mini howto generate keys, certificates and so on. For more details read
the documentation of your favorite key management tool like openssl

# http://www.openssl.org/docs/HOWTO/certificates.txt

#generate ca key + certifcate
openssl req -new -x509 -days 365 -extensions v3_ca -keyout private/cakey.pem -out cacert.pem

#create client/server key + cert request
openssl req -new -keyout private/clientkey.pem -out clientrequest.csr

#CA @ linux:
# 
# create database
echo 01 >CA/serial
touch CA/index.txt

# CA: sign request of client
openssl ca -config ca.cnf -in clientrequest.csr -out clientcert.crt

# CA: sign request of server ! edit server.cnf with right IP address !!!
openssl ca -config ca.cnf -in serverrequest.csr -extfile server.cnf
-extensions dir_sect -out servercert.crt

#convert certificates and keys in DER (ASN.1) format required by matrixssl
openssl rsa -in private/serverkey.pem -inform PEM -out daemon_servkey.pem -outform DER
openssl x509 -in server.crt -inform PEM -out daemon_server.crt -outform DER
openssl x509 -in CA/cacert.pem -inform PEM -out daemon_ca.crt -outform DER
