/**
 * @file   aq.c
 * @Author 02335 team
 * @date   October, 2024
 * @brief  Alarm queue implementation - FIXED v3
 */

#include "aq.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


typedef struct queueNode {
    void *msg;
    MsgKind msgKind;
    struct queueNode *next;
} queueNode;

typedef struct Queue {
    queueNode *head;
    int alarmEnqueued;
    pthread_mutex_t lock;
    pthread_cond_t alarm_received;
    pthread_cond_t message_sent;
} Queue;

queueNode *createQueueNode(void *msg, MsgKind k) {
    queueNode *newQueueNode = malloc(sizeof(queueNode));
    newQueueNode->msg = msg;
    newQueueNode->msgKind = k;
    newQueueNode->next = NULL;
    return newQueueNode;
}

void insertAtEnd(queueNode **head, void *msg, MsgKind k) {
    queueNode *newQueueNode = createQueueNode(msg, k);
    if (*head == NULL) {
        *head = newQueueNode;
        return;
    }
    queueNode *temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newQueueNode;
}

int deleteNode(queueNode **head, int msgContent, void **msg) {
    queueNode **tempP = head;
    queueNode *temp = *head;
    while (temp != NULL) {
        if (*(int *) temp->msg == msgContent) {
            MsgKind retval = temp->msgKind;
            *msg = temp->msg;
            *tempP = temp->next;
            free(temp);
            return retval;
        }
        tempP = &temp->next;
        temp = temp->next;
    }
    return -1;
}

AlarmQueue aq_create() {
    Queue *aq = malloc(sizeof(Queue));
    if (aq != NULL) {
        aq->head = NULL;
        aq->alarmEnqueued = 0;
        pthread_mutex_init(&(aq->lock), NULL);
        pthread_cond_init(&(aq->alarm_received), NULL);
        pthread_cond_init(&(aq->message_sent), NULL);
        return aq;
    }
    return NULL;
}

int aq_send(AlarmQueue aq, void *msg, MsgKind k) {
    if (msg == NULL) return AQ_NULL_MSG;
    if (aq == NULL) return AQ_UNINIT;

    Queue *queue = aq;

    pthread_mutex_lock(&queue->lock);

    // If sending an alarm, wait until no alarm exists in queue
    if (k == AQ_ALARM) {
        while (queue->alarmEnqueued) {
            pthread_cond_wait(&queue->alarm_received, &queue->lock);
        }
        queue->alarmEnqueued = 1;  // Mark alarm as enqueued
    }

    insertAtEnd(&queue->head, msg, k);

    // Signal that a message is available
    pthread_cond_signal(&queue->message_sent);

    pthread_mutex_unlock(&queue->lock);
    return 0;
}

int aq_recv(AlarmQueue aq, void **msg) {
    if (msg == NULL) return AQ_NULL_MSG;
    if (aq == NULL) return AQ_UNINIT;

    Queue *queue = aq;

    pthread_mutex_lock(&queue->lock);

    // Wait until at least one message is available
    while (queue->head == NULL) {
        pthread_cond_wait(&queue->message_sent, &queue->lock);
    }

    queueNode *temp = queue->head;

    // Check if queue has alarm msg
    if (queue->alarmEnqueued) {

        int kind = deleteNode(&queue->head, *(int *) temp-> msg, msg);

        // Clear alarm flag and signal that alarm slot is now free
        queue->alarmEnqueued = 0;

        pthread_cond_signal(&queue->alarm_received);
        pthread_mutex_unlock(&queue->lock);
        return kind;
    }

    // No alarm message found, get first normal message
    temp = queue->head;
    int kind = deleteNode(&queue->head, *(int *) temp->msg, msg);

    pthread_mutex_unlock(&queue->lock);
    return kind;
}

// Not sure we need locks on size functions
// letting them be for now just in case.
int aq_size(AlarmQueue aq) {
    int size = 0;
    Queue *queue = aq;
    pthread_mutex_lock(&queue->lock);

    queueNode *temp = queue->head;

    while (temp != NULL) {
        size++;
        temp = temp->next;
    }
    pthread_mutex_unlock(&queue->lock);
    return size;
}

int aq_alarms(AlarmQueue aq) {
    Queue *queue = aq;
    pthread_mutex_lock(&queue->lock);
    int count = queue->alarmEnqueued;
    pthread_mutex_unlock(&queue->lock);
    return count;
}