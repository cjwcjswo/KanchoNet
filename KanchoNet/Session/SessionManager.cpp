#include "SessionManager.h"
#include "../Utils/Logger.h"

namespace KanchoNet
{
    SessionManager::SessionManager(uint32_t maxSessions)
        : mMaxSessions(maxSessions)
        , mNextSessionID(1) // 0은 INVALID_SESSION_ID
    {
        mSessions.reserve(maxSessions);
    }

    SessionManager::~SessionManager()
    {
        Clear();
    }

    Session* SessionManager::AddSession(SocketHandle socket, const SessionConfig& config)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        if (mSessions.size() >= mMaxSessions)
        {
            LOG_WARNING("Session limit reached. Max: %u", mMaxSessions);
            return nullptr;
        }

        SessionID id = GenerateSessionID();
        auto session = std::make_unique<Session>(id, socket, config);
        Session* sessionPtr = session.get();
        
        mSessions[id] = std::move(session);
        
        LOG_DEBUG("Session added. ID: %llu, Socket: %llu, Total: %zu", 
                  id, socket, mSessions.size());
        
        return sessionPtr;
    }

    bool SessionManager::RemoveSession(SessionID sessionID)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = mSessions.find(sessionID);
        if (it == mSessions.end())
        {
            return false;
        }

        mSessions.erase(it);
        
        LOG_DEBUG("Session removed. ID: %llu, Remaining: %zu", 
                  sessionID, mSessions.size());
        
        return true;
    }

    Session* SessionManager::GetSession(SessionID sessionID)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = mSessions.find(sessionID);
        if (it == mSessions.end())
        {
            return nullptr;
        }

        return it->second.get();
    }

    const Session* SessionManager::GetSession(SessionID sessionID) const
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = mSessions.find(sessionID);
        if (it == mSessions.end())
        {
            return nullptr;
        }

        return it->second.get();
    }

    bool SessionManager::HasSession(SessionID sessionID) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mSessions.find(sessionID) != mSessions.end();
    }

    void SessionManager::ForEachSession(std::function<void(Session*)> callback)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        for (auto& pair : mSessions)
        {
            callback(pair.second.get());
        }
    }

    size_t SessionManager::GetSessionCount() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mSessions.size();
    }

    void SessionManager::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mSessions.clear();
        LOG_INFO("All sessions cleared");
    }

    SessionID SessionManager::GenerateSessionID()
    {
        // 단순 증가 방식 (충돌 없음)
        return mNextSessionID.fetch_add(1, std::memory_order_relaxed);
    }

} // namespace KanchoNet

