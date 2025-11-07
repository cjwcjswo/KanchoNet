#pragma once

#include "../Types.h"
#include "Session.h"
#include "../Utils/NonCopyable.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace KanchoNet
{
    // 세션 컨테이너 관리 클래스
    class SessionManager : public NonCopyable
    {
    public:
        explicit SessionManager(uint32_t maxSessions);
        ~SessionManager();

        // 세션 추가
        Session* AddSession(SocketHandle socket, const SessionConfig& config);
        
        // 세션 제거
        bool RemoveSession(SessionID sessionID);
        
        // 세션 검색
        Session* GetSession(SessionID sessionID);
        const Session* GetSession(SessionID sessionID) const;
        
        // 세션 존재 확인
        bool HasSession(SessionID sessionID) const;
        
        // 전체 세션 순회
        void ForEachSession(std::function<void(Session*)> callback);
        
        // 상태 정보
        size_t GetSessionCount() const;
        size_t GetMaxSessions() const { return maxSessions_; }
        bool IsFull() const { return GetSessionCount() >= maxSessions_; }
        
        // 전체 세션 제거
        void Clear();

    private:
        SessionID GenerateSessionID();

        uint32_t maxSessions_;
        std::atomic<SessionID> nextSessionID_;
        
        std::unordered_map<SessionID, std::unique_ptr<Session>> sessions_;
        mutable std::mutex mutex_;
    };

} // namespace KanchoNet

