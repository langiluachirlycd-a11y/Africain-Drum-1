#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

// Offline licence: a key is HMAC-SHA256(secret, "<name>|<email>") shown as AGD-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX.
// Keys are made with Tools/License/keygen.py using the same secret that the plugin is built with
// (CMake: -DAG_LICENSE_SECRET=... -DAG_REQUIRE_LICENSE=ON).  This is a deterrent, not unbreakable protection.
#ifndef DF_LICENSE_SECRET
 #define DF_LICENSE_SECRET "CHANGE-THIS-SECRET-BEFORE-RELEASE"
#endif
#ifndef DF_REQUIRE_LICENSE
 #define DF_REQUIRE_LICENSE 0
#endif

namespace df
{
    struct License
    {
        struct Info { juce::String name, email, key; bool valid = false; };

        static juce::File file()
        {
            return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("African Gospel").getChildFile ("African Drum").getChildFile ("license.key");
        }

        static juce::MemoryBlock sha (const juce::MemoryBlock& m)
        {
            return juce::SHA256 (m.getData(), m.getSize()).getRawData();
        }

        static juce::String hmacHex (const juce::String& secret, const juce::String& message)
        {
            juce::MemoryBlock key (secret.toRawUTF8(), secret.getNumBytesAsUTF8());
            if (key.getSize() > 64)
                key = sha (key);
            key.setSize (64, true);

            juce::MemoryBlock inner, outer;
            for (size_t i = 0; i < 64; ++i)
            {
                const auto kb = (juce::uint8) key[i];
                const juce::uint8 ip = (juce::uint8) (kb ^ 0x36), op = (juce::uint8) (kb ^ 0x5c);
                inner.append (&ip, 1);
                outer.append (&op, 1);
            }

            inner.append (message.toRawUTF8(), message.getNumBytesAsUTF8());
            const auto ih = sha (inner);
            outer.append (ih.getData(), ih.getSize());
            const auto oh = sha (outer);
            return juce::String::toHexString (oh.getData(), (int) oh.getSize(), 0);
        }

        static juce::String makeKey (const juce::String& name, const juce::String& email)
        {
            const auto hex = hmacHex (DF_LICENSE_SECRET, name.trim().toLowerCase() + "|" + email.trim().toLowerCase())
                                 .toUpperCase().substring (0, 25);
            juce::String k = "AGD";
            for (int i = 0; i < 25; i += 5)
                k << "-" << hex.substring (i, i + 5);
            return k;
        }

        static bool verify (const juce::String& name, const juce::String& email, const juce::String& key)
        {
            return name.trim().isNotEmpty() && email.trim().isNotEmpty()
                && key.trim().toUpperCase() == makeKey (name, email);
        }

        static Info load()
        {
            Info i;
            const auto f = file();
            if (! f.existsAsFile())
                return i;

            auto lines = juce::StringArray::fromLines (f.loadFileAsString());
            if (lines.size() >= 3)
            {
                i.name = lines[0].trim(); i.email = lines[1].trim(); i.key = lines[2].trim();
                i.valid = verify (i.name, i.email, i.key);
            }
            return i;
        }

        static bool save (const juce::String& name, const juce::String& email, const juce::String& key)
        {
            const auto f = file();
            f.getParentDirectory().createDirectory();
            return f.replaceWithText (name.trim() + "\n" + email.trim() + "\n" + key.trim().toUpperCase() + "\n");
        }
    };
}
