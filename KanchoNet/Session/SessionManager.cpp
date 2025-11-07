#include "SessionManager.h"
#include "../Utils/Logger.h"

namespace KanchoNet
{
    SessionManager::SessionManager(uint32_t maxSessions)
        : maxSessions_(maxSessions)
        , nextSessionID_(1) // 0은 INVALID_SESSION_ID
    {
        sessions_.reserve(maxSessions);
    }

    SessionManager::~SessionManager()
    {
        Clear();
    }

    Session* SessionManager::AddSession(SocketHandle socket, const SessionConfig& config)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (sessions_.size() >= maxSessions_)
        {
            LOG_WARNING("Session limit reached. Max: %u", maxSessions_);
            return nullptr;
        }

        SessionID id = GenerateSessionID();
        auto session = std::make_unique<Session>(id, socket, config);
        Session* sessionPtr = session.get();
        
        sessions_[id] = std::move(session);
        
        LOG_DEBUG("Session added. ID: %llu, Socket: %llu, Total: %zu", 
                  id, socket, sessions_.size());
        
        return sessionPtr;
    }

    bool SessionManager::RemoveSession(SessionID sessionID)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(sessionID);
        if (it == sessions_.end())
        {
            return false;
        }

        sessions_.erase(it);
        
        LOG_DEBUG("Session removed. ID: %llu, Remaining: %zu", 
                  sessionID, sessions_.size());
        
        return true;
    }

    Session* SessionManager::GetSession(SessionID sessionID)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(sessionID);
        if (it == sessions_.end())
        {
            return nullptr;
        }

        return it->second.get();
    }

    const Session* SessionManager::GetSession(SessionID sessionID) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(sessionID);
        if (it == sessions_.end())
        {
            return nullptr;
        }

        return it->second.get();
    }

    bool SessionManager::HasSession(SessionID sessionID) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.find(sessionID) != sessions_.end();
    }

    void SessionManager::ForEachSession(std::function<void(Session*)> callback)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& pair : sessions_)
        {
            callback(pair.second.get());
        }
    }

    size_t SessionManager::GetSessionCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

    void SessionManager::Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.clear();
        LOG_INFO("All sessions cleared");
    }

    SessionID SessionManager::GenerateSessionID()
    {
        // 단순 증가 방식 (충돌 없음)
        return nextSessionID_.fetch_add(1, std::memory_order_relaxed);
    }

} // namespace KanchoNet

