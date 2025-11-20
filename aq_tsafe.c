/**
 * @file   aq.tsafe
 * @Author 02335 team
 * @date   October, 2024
 * @brief  Thread-safe Alarm Queue implementation
 */

#include "aq.h"
#include <stdlib.h>
#include <pthread.h>

/* ---------------- NODE STRUCT ---------------- */

typedef struct queueNode {
    void *msg;
    MsgKind msgKind;
    struct queueNode *next;
} queueNode;

/* ---------------- QUEUE STRUCT ---------------- */

typedef struct AQ {
    queueNode *head;
    pthread_mutex_t lock;
    pthread_cond_t notEmpty;
    pthread_cond_t alarmFree;
} AQ;

/* ---------------- HELPERS ---------------- */

static queueNode *createNode(void *msg, MsgKind k) {
    queueNode *n = malloc(sizeof(queueNode));
    n->msg = msg;
    n->msgKind = k;
    n->next = NULL;
    return n;
}

static int deleteNode(queueNode **head, queueNode *target, void **msgOut) {
    queueNode **pp = head;
    queueNode *cur = *head;

    while (cur != NULL) {
        if (cur == target) {
            *pp = cur->next;
            *msgOut = cur->msg;
            int kind = cur->msgKind;
            free(cur);
            return kind;
        }
        pp = &cur->next;
        cur = cur->next;
    }
    return AQ_NO_MSG;
}



AlarmQueue aq_create() {
    AQ *q = malloc(sizeof(AQ));
    if (!q) return NULL;

    q->head = NULL;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
    pthread_cond_init(&q->alarmFree, NULL);

    return (AlarmQueue) q;
}

/* ---------------- SEND ---------------- */

int aq_send(AlarmQueue aq, void *msg, MsgKind k) {
    if (!aq) return AQ_UNINIT;
    if (!msg) return AQ_NULL_MSG;

    AQ *q = (AQ*) aq;

    pthread_mutex_lock(&q->lock);

    /* If sending an alarm: wait until NO alarm exists */
    if (k == AQ_ALARM) {
        int alarmExists = 1;
        while (alarmExists) {
            alarmExists = 0;
            for (queueNode *t = q->head; t; t = t->next) {
                if (t->msgKind == AQ_ALARM) {
                    alarmExists = 1;
                    break;
                }
            }
            if (alarmExists)
                pthread_cond_wait(&q->alarmFree, &q->lock);
        }
    }

    /* Insert new message */
    int wasEmpty = (q->head == NULL);

    queueNode *n = createNode(msg, k);
    if (q->head == NULL) {
        q->head = n;
    } else {
        queueNode *t = q->head;
        while (t->next) t = t->next;
        t->next = n;
    }

    if (wasEmpty)
        pthread_cond_signal(&q->notEmpty);

    pthread_mutex_unlock(&q->lock);
    return 0;
}

/* ---------------- RECEIVE ---------------- */

int aq_recv(AlarmQueue aq, void **msgOut) {
    if (!msgOut) return AQ_NULL_MSG;
    if (!aq) return AQ_UNINIT;

    AQ *q = (AQ*) aq;
    pthread_mutex_lock(&q->lock);

    while (q->head == NULL)
        pthread_cond_wait(&q->notEmpty, &q->lock);

    /* Priority: alarms first */
    for (queueNode *t = q->head; t; t = t->next) {
        if (t->msgKind == AQ_ALARM) {
            int kind = deleteNode(&q->head, t, msgOut);
            pthread_cond_signal(&q->alarmFree);
            pthread_mutex_unlock(&q->lock);
            return kind;
        }
    }

    /* Otherwise: remove head */
    queueNode *first = q->head;
    int kind = deleteNode(&q->head, first, msgOut);

    pthread_mutex_unlock(&q->lock);
    return kind;
}

/* ---------------- SIZE ---------------- */

int aq_size(AlarmQueue aq) {
    if (!aq) return AQ_UNINIT;

    AQ *q = (AQ*) aq;
    pthread_mutex_lock(&q->lock);

    int count = 0;
    for (queueNode *t = q->head; t; t = t->next)
        count++;

    pthread_mutex_unlock(&q->lock);
    return count;
}

/* ---------------- ALARM COUNT ---------------- */

int aq_alarms(AlarmQueue aq) {
    if (!aq) return AQ_UNINIT;

    AQ *q = (AQ*) aq;
    pthread_mutex_lock(&q->lock);

    int count = 0;
    for (queueNode *t = q->head; t; t = t->next)
        if (t->msgKind == AQ_ALARM)
            count++;

    pthread_mutex_unlock(&q->lock);
    return count;
}
