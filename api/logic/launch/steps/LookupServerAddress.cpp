#include "LookupServerAddress.h"

#include <launch/LaunchTask.h>

LookupServerAddress::LookupServerAddress(LaunchTask *parent) :
    LaunchStep(parent), m_dnsLookup(new QDnsLookup(this))
{
    connect(m_dnsLookup, &QDnsLookup::finished, this, &LookupServerAddress::on_dnsLookupFinished);

    m_dnsLookup->setType(QDnsLookup::SRV);
}

void LookupServerAddress::setLookupAddress(const QString &lookupAddress)
{
    m_lookupAddress = lookupAddress;
    m_dnsLookup->setName(QString("_minecraft._tcp.%1").arg(lookupAddress));
}

void LookupServerAddress::setPort(quint16 port)
{
    m_port = port;
}

void LookupServerAddress::setOutputAddressPtr(MinecraftServerTargetPtr output)
{
    m_output = std::move(output);
}

bool LookupServerAddress::abort()
{
    m_dnsLookup->abort();
    emitFailed("Aborted");
    return true;
}

void LookupServerAddress::executeTask()
{
    m_dnsLookup->lookup();
}

void LookupServerAddress::on_dnsLookupFinished()
{
    if (isFinished())
    {
        // Aborted
        return;
    }

    if (m_dnsLookup->error() != QDnsLookup::NoError)
    {
        emit logLine(QString("Failed to resolve server address (this is NOT an error!) %1: %2\n")
            .arg(m_dnsLookup->name(), m_dnsLookup->errorString()), MessageLevel::MultiMC);
        resolve(m_lookupAddress, m_port); // Technically the task failed, however, we don't abort the launch
                                                      // and leave it up to minecraft to fail (or maybe not) when connecting
        return;
    }

    const auto records = m_dnsLookup->serviceRecords();
    if (records.empty())
    {
        emit logLine(
                QString("Failed to resolve server address %1: the DNS lookup succeeded, but no records were returned.\n")
                .arg(m_dnsLookup->name()), MessageLevel::Warning);
        resolve(m_lookupAddress, m_port); // Technically the task failed, however, we don't abort the launch
                                                      // and leave it up to minecraft to fail (or maybe not) when connecting
        return;
    }

    const auto &firstRecord = records.at(0);

    if (firstRecord.port() != m_port && m_port != 0)
    {
        emit logLine(
                QString("DNS record for %1 suggested %2 as server port, but user supplied %3. Using user override,"
                        " but the port may be wrong!\n").arg(m_dnsLookup->name(), QString::number(firstRecord.port()), QString::number(m_port)),
                        MessageLevel::Warning);
    }
    else if (m_port == 0)
    {
        m_port = firstRecord.port();
    }

    emit logLine(QString("Resolved server address %1 to %2 with port %3\n").arg(
            m_dnsLookup->name(), firstRecord.target(), QString::number(m_port)),MessageLevel::MultiMC);
    resolve(firstRecord.target(), m_port);
}

void LookupServerAddress::resolve(const QString &address, quint16 port)
{
    m_output->address = address;
    m_output->port = port;

    emitSucceeded();
    m_dnsLookup->deleteLater();
}
