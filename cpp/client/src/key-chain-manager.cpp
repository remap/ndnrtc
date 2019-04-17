//
// key-chain-manager.cpp
//
//  Created by Peter Gusev on 02 September 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "key-chain-manager.hpp"
#include <algorithm>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/config-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <ndn-cpp/security/v2/certificate-cache-v2.hpp>
#include <ndn-cpp/security/pib/pib-memory.hpp>
#include <ndn-cpp/security/tpm/tpm-back-end-memory.hpp>
#include <ndn-cpp/security/signing-info.hpp>
#include <ndn-cpp/face.hpp>
#include <ndnrtc/simple-log.hpp>
#include <boost/chrono.hpp>

using namespace boost::chrono;
using namespace ndn;

KeyChainManager::KeyChainManager(std::shared_ptr<Face> face,
                                 const std::string &identityNameStr,
                                 const std::string &instanceNameStr,
                                 const std::string &configPolicy,
                                 unsigned int runTime) 
    : face_(face),
    signingIdentity_(identityNameStr),
    instanceName_(instanceNameStr),
    configPolicy_(configPolicy),
    runTime_(runTime)
{
    checkExists(configPolicy);
    setupDefaultKeyChain();
    setupConfigPolicyManager();
    setupInstanceKeyChain();
}

void KeyChainManager::setupDefaultKeyChain()
{
    defaultKeyChain_ = std::make_shared<ndn::KeyChain>();
}

void KeyChainManager::setupInstanceKeyChain()
{
    Name signingIdentity(signingIdentity_);
    std::vector<Name> identities;

    if (defaultKeyChain_->getIsSecurityV1())
    {
        defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, false);
        defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, true);
    }
    else
        defaultKeyChain_->getPib().getAllIdentityNames(identities);

    if (std::find(identities.begin(), identities.end(), signingIdentity) == identities.end())
    {
        // create signing identity in default keychain
        LogInfo("") << "Signing identity not found. Creating..." << std::endl;
        createSigningIdentity();
    }

    createMemoryKeychain();
    if (defaultKeyChain_->getIsSecurityV1())
        createInstanceIdentity();
    else
        createInstanceIdentityV2();
}

void KeyChainManager::setupConfigPolicyManager()
{
    identityStorage_ = std::make_shared<MemoryIdentityStorage>();
    privateKeyStorage_ = std::make_shared<MemoryPrivateKeyStorage>();
    if (defaultKeyChain_->getIsSecurityV1())
        configPolicyManager_ = std::make_shared<ConfigPolicyManager>(configPolicy_);
    else
        configPolicyManager_ = std::make_shared<ConfigPolicyManager>(configPolicy_, std::make_shared<CertificateCacheV2>());
}

void KeyChainManager::createSigningIdentity()
{
    // create self-signed certificate
    Name cert = defaultKeyChain_->createIdentityAndCertificate(Name(signingIdentity_));

    LogWarn("") << "Generated identity " << signingIdentity_ << " (certificate name "
                << cert << ")" << std::endl;
    LogWarn("") << "Check policy config file for correct trust anchor (run `ndnsec-dump-certificate -i "
                << signingIdentity_ << " > signing.cert` if needed)" << std::endl;
}

void KeyChainManager::createMemoryKeychain()
{
    if (defaultKeyChain_->getIsSecurityV1())
        instanceKeyChain_ = std::make_shared<KeyChain>(std::make_shared<IdentityManager>(identityStorage_, privateKeyStorage_),
                                                         configPolicyManager_);
    else
        instanceKeyChain_ = std::make_shared<KeyChain>(std::make_shared<PibMemory>(), std::make_shared<TpmBackEndMemory>(),
                                                         configPolicyManager_);

    instanceKeyChain_->setFace(face_.get());
}

void KeyChainManager::createInstanceIdentity()
{
    auto now = system_clock::now().time_since_epoch();
    duration<double> sec = now;

    Name instanceIdentity(signingIdentity_);

    instanceIdentity.append(instanceName_);
    instanceIdentity_ = instanceIdentity.toUri();

    LogInfo("") << "Instance identity " << instanceIdentity << std::endl;

    Name instanceKeyName = instanceKeyChain_->generateRSAKeyPairAsDefault(instanceIdentity, true);
    Name signingCert = defaultKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(signingIdentity_));

    LogDebug("") << "Instance key " << instanceKeyName << std::endl;
    LogDebug("") << "Signing certificate " << signingCert << std::endl;

    std::vector<CertificateSubjectDescription> subjectDescriptions;
    instanceCert_ =
        instanceKeyChain_->getIdentityManager()->prepareUnsignedIdentityCertificate(instanceKeyName,
                                                                                    Name(signingIdentity_),
                                                                                    sec.count() * 1000,
                                                                                    (sec + duration<double>(runTime_)).count() * 1000,
                                                                                    subjectDescriptions,
                                                                                    &instanceIdentity);
    assert(instanceCert_.get());

    defaultKeyChain_->sign(*instanceCert_, signingCert);
    instanceKeyChain_->installIdentityCertificate(*instanceCert_);
    instanceKeyChain_->setDefaultCertificateForKey(*instanceCert_);
    instanceKeyChain_->getIdentityManager()->setDefaultIdentity(instanceIdentity);

    LogInfo("") << "Instance certificate "
                << instanceKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(instanceIdentity)) << std::endl;
}

void KeyChainManager::createInstanceIdentityV2()
{
    auto now = system_clock::now().time_since_epoch();
    duration<double> sec = now;

    Name instanceIdentity(signingIdentity_);

    instanceIdentity.append(instanceName_);
    instanceIdentity_ = instanceIdentity.toUri();

    LogInfo("") << "Instance identity " << instanceIdentity << std::endl;

    std::shared_ptr<PibIdentity> instancePibIdentity =
        instanceKeyChain_->createIdentityV2(instanceIdentity);
    std::cout << "Debug instancePibIdentity " << instancePibIdentity->getName() << std::endl;
    std::shared_ptr<PibKey> instancePibKey =
        instancePibIdentity->getDefaultKey();
    std::shared_ptr<PibKey> signingPibKey = defaultKeyChain_->getPib()
                                                  .getIdentity(Name(signingIdentity_))
                                                  ->getDefaultKey();
    Name signingCert = signingPibKey->getDefaultCertificate()->getName();

    LogDebug("") << "Instance key " << instancePibKey->getName() << std::endl;
    LogDebug("") << "Signing certificate " << signingCert << std::endl;

    // Prepare the instance certificate.
    std::shared_ptr<CertificateV2> instanceCertificate(new CertificateV2());
    Name certificateName = instancePibKey->getName();
    // Use the issuer's public key digest.
    certificateName.append(PublicKey(signingPibKey->getPublicKey()).getDigest());
    certificateName.appendVersion((uint64_t)(sec.count() * 1000));
    instanceCertificate->setName(certificateName);
    instanceCertificate->getMetaInfo().setType(ndn_ContentType_KEY);
    instanceCertificate->getMetaInfo().setFreshnessPeriod(3600 * 1000.0);
    instanceCertificate->setContent(instancePibKey->getPublicKey());

    SigningInfo signingParams(signingPibKey);
    signingParams.setValidityPeriod(ValidityPeriod(sec.count() * 1000,
                                                   (sec + duration<double>(runTime_)).count() * 1000));
    defaultKeyChain_->sign(*instanceCertificate, signingParams);

    instanceKeyChain_->addCertificate(*instancePibKey, *instanceCertificate);
    instanceKeyChain_->setDefaultCertificate(*instancePibKey, *instanceCertificate);
    instanceKeyChain_->setDefaultIdentity(*instancePibIdentity);
    instanceCert_ = instanceCertificate;

    LogInfo("") << "Instance certificate "
                << instanceKeyChain_->getPib().getIdentity(Name(instanceIdentity_))->getDefaultKey()->getDefaultCertificate()->getName() << std::endl;
}

void KeyChainManager::checkExists(const std::string &file)
{
    std::ifstream stream(file.c_str());
    bool result = (bool)stream;
    stream.close();

    if (!result)
    {
        std::stringstream ss;
        ss << "Can't find file " << file << std::endl;
        throw std::runtime_error(ss.str());
    }
}
